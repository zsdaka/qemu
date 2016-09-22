#!/usr/bin/env python
# -*- coding: utf-8 -*-

"""
trace/generated-events.c
"""

__author__     = "Lluís Vilanova <vilanova@ac.upc.edu>"
__copyright__  = "Copyright 2012-2016, Lluís Vilanova <vilanova@ac.upc.edu>"
__license__    = "GPL version 2 or (at your option) any later version"

__maintainer__ = "Stefan Hajnoczi"
__email__      = "stefanha@linux.vnet.ibm.com"


from tracetool import out


def generate(events, backend):
    out('/* This file is autogenerated by tracetool, do not edit. */',
        '',
        '#include "qemu/osdep.h"',
        '#include "trace.h"',
        '#include "trace/generated-events.h"',
        '#include "trace/control.h"',
        '')

    for e in events:
        out('uint16_t %s;' % e.api(e.QEMU_DSTATE))

    for e in events:
        if "vcpu" in e.properties:
            vcpu_id = 0
        else:
            vcpu_id = "TRACE_VCPU_EVENT_NONE"
        out('TraceEvent %(event)s = {',
            '  .id = 0,',
            '  .vcpu_id = %(vcpu_id)s,',
            '  .name = \"%(name)s\",',
            '  .sstate = %(sstate)s,',
            '  .dstate = &%(dstate)s ',
            '};',
            event = e.api(e.QEMU_EVENT),
            vcpu_id = vcpu_id,
            name = e.name,
            sstate = "TRACE_%s_ENABLED" % e.name.upper(),
            dstate = e.api(e.QEMU_DSTATE))

    out('TraceEvent *trace_events[] = {')

    for e in events:
        out('&%(event)s,', event = e.api(e.QEMU_EVENT))

    out('  NULL,',
        '};',
        '')

    out('static void trace_register_events(void)',
        '{',
        '    trace_event_register_group(trace_events);',
        '}',
        'trace_init(trace_register_events)')
