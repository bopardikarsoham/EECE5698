// info_layout.h - layout of the per-shot info record the kernel writes (shared by the
// kernel, the host program and the testbench).
//
// Every shot gets INFO_WORDS 32-bit words in the info buffer:
//   INFO_STATUS     (status << 16) | growth iterations; status 0 = OK, 1 = cap hit,
//                   2 = invalid correction. 0xFFFFFFFF = no usable graph loaded.
//   INFO_WORK       work: inner-loop iterations the shot needed (cycle proxy)
//   INFO_DEFECTS    fired detectors in the shot
//   INFO_MEMBERS .. INFO_CLEANUP
//                   loop iterations of each decoder stage (uf::StageCounts), counted
//                   by the kernel itself

#ifndef INFO_LAYOUT_H
#define INFO_LAYOUT_H

enum InfoWord {
    INFO_STATUS = 0,
    INFO_WORK,
    INFO_DEFECTS,
    INFO_MEMBERS,
    INFO_FRONTIER,
    INFO_APPLY,
    INFO_MERGES,
    INFO_RELABEL,
    INFO_COMPACT,
    INFO_PEEL_SCAN,
    INFO_PEEL_WALK,
    INFO_CLEANUP,
    INFO_WORDS
};

// Column names for the stage counters, in InfoWord order from INFO_DEFECTS.
static const char* const INFO_STAGE_NAMES[] = {
    "defects", "members", "frontier", "apply", "merges",
    "relabel", "compact", "peel_scan", "peel_walk", "cleanup"};

#endif  // INFO_LAYOUT_H
