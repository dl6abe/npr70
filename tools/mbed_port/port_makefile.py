#!/usr/bin/env python3
"""Merge the mbed-os part of an exported Makefile into the project Makefile.

The project Makefile is an old mbed export of mbed-os 5.6.6 with project settings added
(FW_BUILD_ID, flags, source objects). A newer mbed-os moves and adds files, so the list of
mbed-os objects, the mbed-os include paths, the linker script and a few -D flags must come from
a fresh export of the same mbed-os version (export_mbed.sh).

Taken from the export: mbed-os objects of the core directories only (no BLE, LWIP, cellular,
storage ...), their include paths, LINKER_SCRIPT, and -D flags the project Makefile lacks.
Kept from the project: everything else. mbed_config.h: the project file plus the non-network
macros the export adds.

usage: port_makefile.py EXPORT_DIR OUT_DIR [PROJECT_DIR]
  EXPORT_DIR   export-TAG directory made by export_mbed.sh (Makefile, mbed_config.h)
  OUT_DIR      receives Makefile and mbed_config.h
  PROJECT_DIR  directory with the project Makefile and mbed_config.h (default: npr70/)
"""
import os
import re
import sys

CORE = ('mbed-os/drivers', 'mbed-os/hal', 'mbed-os/platform', 'mbed-os/rtos', 'mbed-os/cmsis',
        'mbed-os/events', 'mbed-os/targets/TARGET_STM')
# macros that only describe network, storage and radio stacks, not needed for this firmware
NETWORK = re.compile(r'NSAPI|LWIP|CELLULAR|BLE|NANOSTACK|ESP8266|PPP|MBED_TRACE|STORAGE|'
                     r'FILESYSTEM|MBED_CONF_SD|LORA|UBLOX|MBED_LFS|NVSTORE|UNITY|PLATFORM_CALLBACK|'
                     r'PLATFORM_MAX|PLATFORM_FORCE|MESH_API|6LOWPAN|MBED_CONF_THREAD|WISUN|MBED_CONF_MBED_CLIENT|MBED_CONF_NANOSTACK')
KEEP_DEFINE = re.compile(r'MBED_BUILD_TIMESTAMP')


# objects of mbed-os that the firmware does not use and that need parts of mbed-os we do not copy
EXCLUDE = re.compile(r'/usb/|/USB[A-Za-z]*\.o|mbed_pinmap_default')


def norm(path):
    """Path from the export, relative to its build directory, reduced to mbed-os/..."""
    m = re.search(r'(?:^|/)(mbed-os(?:/.*)?)$', path)
    if m:
        return re.sub(r'/\.$', '', m.group(1))
    return re.sub(r'^(\./|(\.\./)+|\.\./\./)+', '', path)


def read(path):
    with open(path) as f:
        return f.read().split('\n')


def main():
    export_dir, out_dir = sys.argv[1], sys.argv[2]
    here = os.path.dirname(os.path.abspath(__file__))
    project = sys.argv[3] if len(sys.argv) > 3 else os.path.normpath(os.path.join(here, '..', '..'))
    os.makedirs(out_dir, exist_ok=True)

    ours = read(os.path.join(project, 'Makefile'))
    theirs = read(os.path.join(export_dir, 'Makefile'))

    # objects, include paths, linker script, defines of the export
    e_obj, e_inc, e_def, e_ld = [], [], {}, None
    for line in theirs:
        m = re.match(r'OBJECTS \+= (\S+)\.o\s*$', line)
        if m and norm(m.group(1)).startswith(CORE) and not EXCLUDE.search(m.group(1) + '.o'):
            e_obj.append('OBJECTS += ./' + norm(m.group(1)) + '.o')
        m = re.match(r'INCLUDE_PATHS \+= -I(\S+)\s*$', line)
        if m and (norm(m.group(1)) == 'mbed-os' or norm(m.group(1)).startswith(CORE)):
            e_inc.append('INCLUDE_PATHS += -I.././' + norm(m.group(1)))
        m = re.match(r'(C|CXX|ASM)_FLAGS \+= -D(\S+)\s*$', line)
        if m:
            e_def.setdefault(m.group(1), []).append(m.group(2))
        m = re.match(r'LINKER_SCRIPT \?= (\S+)\s*$', line)
        if m:
            e_ld = '.././' + norm(m.group(1))

    # project Makefile with the mbed-os parts replaced
    out, obj_done, inc_done = [], False, False
    have_def = {}
    for line in ours:
        m = re.match(r'(C|CXX|ASM)_FLAGS \+= -D(\S+)\s*$', line)
        if m:
            have_def.setdefault(m.group(1), set()).add(m.group(2))
        if re.match(r'OBJECTS \+= \./mbed-os/', line):
            if not obj_done:
                out.extend(sorted(set(e_obj)))
                obj_done = True
            continue
        if re.match(r'INCLUDE_PATHS \+= -I\.\./\./mbed-os', line):
            if not inc_done:
                out.extend(sorted(set(e_inc)))
                inc_done = True
            continue
        if e_ld and line.startswith('LINKER_SCRIPT ?='):
            line = 'LINKER_SCRIPT ?= ' + e_ld
        out.append(line)

    added = {}
    for kind, names in e_def.items():
        for name in names:
            if name in have_def.get(kind, set()) or KEEP_DEFINE.search(name):
                continue
            idx = [i for i, l in enumerate(out) if l.startswith(kind + '_FLAGS += -D')]
            if idx:
                out.insert(idx[-1] + 1, kind + '_FLAGS += -D' + name)
                added.setdefault(kind, []).append(name)
    removed = {k: sorted(v - set(e_def.get(k, []))) for k, v in have_def.items()}

    # keep the line endings of the project files (the project Makefile uses CRLF)
    def crlf(path):
        with open(path, 'rb') as f:
            return b'\r\n' in f.read()
    eol = '\r\n' if crlf(os.path.join(project, 'Makefile')) else '\n'
    with open(os.path.join(out_dir, 'Makefile'), 'w', newline=eol) as f:
        f.write('\n'.join(out))

    # mbed_config.h: project file plus the non-network macros of the export
    def macros(lines):
        d = {}
        for l in lines:
            # the value may hold spaces and quotes (a string); the comment "// set by ..." follows it
            m = re.match(r'#define\s+(\S+)\s+(.*?)\s*(?://\s*set by.*)?$', l)
            if m:
                d[m.group(1)] = m.group(2)
        return d
    cfg = read(os.path.join(project, 'mbed_config.h'))
    have = macros(cfg)
    new = {k: v for k, v in macros(read(os.path.join(export_dir, 'mbed_config.h'))).items()
           if k not in have and not NETWORK.search(k)}
    end = max(i for i, l in enumerate(cfg) if l.startswith('#endif'))
    cfg[end:end] = ['#define %-49s %s // from export' % kv for kv in sorted(new.items())]
    eol = '\r\n' if crlf(os.path.join(project, 'mbed_config.h')) else '\n'
    with open(os.path.join(out_dir, 'mbed_config.h'), 'w', newline=eol) as f:
        f.write('\n'.join(cfg))

    print('objects from export: %d, include paths: %d' % (len(set(e_obj)), len(set(e_inc))))
    print('linker script:', e_ld)
    print('-D added to the Makefile:', added or 'none')
    print('-D in the project Makefile but not in the export (kept, check):', removed)
    print('macros added to mbed_config.h:', sorted(new) or 'none')


if __name__ == '__main__':
    main()
