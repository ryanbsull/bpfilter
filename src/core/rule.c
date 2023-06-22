/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 Meta Platforms, Inc. and affiliates.
 */

#include "core/rule.h"

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "core/list.h"
#include "core/logger.h"
#include "core/marsh.h"
#include "core/match.h"
#include "core/target.h"
#include "shared/helper.h"

int bf_rule_new(struct bf_rule **rule)
{
    struct bf_rule *_rule;

    _rule = calloc(1, sizeof(*_rule));
    if (!_rule)
        return -ENOMEM;

    bf_list_init(&_rule->matches,
                 (bf_list_ops[]) {{.free = (bf_list_ops_free)bf_match_free}});

    *rule = _rule;

    return 0;
}

void bf_rule_free(struct bf_rule **rule)
{
    if (!*rule)
        return;

    bf_list_clean(&(*rule)->matches);
    bf_target_free(&(*rule)->target);

    free(*rule);
    *rule = NULL;
}

int bf_rule_marsh(const struct bf_rule *rule, struct bf_marsh **marsh)
{
    _cleanup_bf_marsh_ struct bf_marsh *_marsh = NULL;
    int r;

    assert(rule);
    assert(marsh);

    r = bf_marsh_new(&_marsh, NULL, 0);
    if (r < 0)
        return r;

    r |= bf_marsh_add_child_raw(&_marsh, &rule->index, sizeof(rule->index));
    r |= bf_marsh_add_child_raw(&_marsh, &rule->ifindex, sizeof(rule->ifindex));
    r |= bf_marsh_add_child_raw(&_marsh, &rule->invflags,
                                sizeof(rule->invflags));
    r |= bf_marsh_add_child_raw(&_marsh, &rule->src, sizeof(rule->src));
    r |= bf_marsh_add_child_raw(&_marsh, &rule->dst, sizeof(rule->dst));
    r |= bf_marsh_add_child_raw(&_marsh, &rule->src_mask,
                                sizeof(rule->src_mask));
    r |= bf_marsh_add_child_raw(&_marsh, &rule->dst_mask,
                                sizeof(rule->dst_mask));
    r |= bf_marsh_add_child_raw(&_marsh, &rule->protocol,
                                sizeof(rule->protocol));
    r |=
        bf_marsh_add_child_raw(&_marsh, rule->target, sizeof(struct bf_target));
    if (r)
        return bf_err_code(r, "Failed to serialize rule");

    *marsh = TAKE_PTR(_marsh);

    return 0;
}

int bf_rule_unmarsh(const struct bf_marsh *marsh, struct bf_rule **rule)
{
    _cleanup_bf_rule_ struct bf_rule *_rule = NULL;
    _cleanup_bf_target_ struct bf_target *target = NULL;
    struct bf_marsh *child = NULL;
    int r;

    assert(marsh);
    assert(rule);

    r = bf_rule_new(&_rule);
    if (r < 0)
        return r;

    child = bf_marsh_next_child(marsh, NULL);

    memcpy(&_rule->index, child->data, sizeof(_rule->index));
    child = bf_marsh_next_child(marsh, child);

    memcpy(&_rule->ifindex, child->data, sizeof(_rule->ifindex));
    child = bf_marsh_next_child(marsh, child);

    memcpy(&_rule->invflags, child->data, sizeof(_rule->invflags));
    child = bf_marsh_next_child(marsh, child);

    memcpy(&_rule->src, child->data, sizeof(_rule->src));
    child = bf_marsh_next_child(marsh, child);

    memcpy(&_rule->dst, child->data, sizeof(_rule->dst));
    child = bf_marsh_next_child(marsh, child);

    memcpy(&_rule->src_mask, child->data, sizeof(_rule->src_mask));
    child = bf_marsh_next_child(marsh, child);

    memcpy(&_rule->dst_mask, child->data, sizeof(_rule->dst_mask));
    child = bf_marsh_next_child(marsh, child);

    memcpy(&_rule->protocol, child->data, sizeof(_rule->protocol));
    child = bf_marsh_next_child(marsh, child);

    r = bf_target_new(&target);
    if (r < 0)
        return bf_err_code(r, "Failed to create target");

    memcpy(target, child->data, sizeof(struct bf_target));
    _rule->target = TAKE_PTR(target);

    if (bf_marsh_next_child(marsh, child))
        bf_warn("codegen marsh has more children than expected");

    *rule = TAKE_PTR(_rule);

    return 0;
}

void bf_rule_dump(const struct bf_rule *rule, prefix_t *prefix)
{
    prefix_t _prefix = {};
    prefix = prefix ?: &_prefix;

    DUMP(prefix, "struct bf_rule at %p", rule);

    bf_dump_prefix_push(prefix);

    DUMP(prefix, "index: %u", rule->index);
    DUMP(prefix, "ifindex: %u", rule->ifindex);
    DUMP(prefix, "invflags: %u", rule->invflags);
    DUMP(prefix, "src: " IP4_FMT, IP4_SPLIT(rule->src));
    DUMP(prefix, "dst: " IP4_FMT, IP4_SPLIT(rule->dst));
    DUMP(prefix, "src_mask: " IP4_FMT, IP4_SPLIT(rule->src_mask));
    DUMP(prefix, "dst_mask: " IP4_FMT, IP4_SPLIT(rule->dst_mask));
    DUMP(prefix, "protocol: %u", rule->protocol);
    DUMP(prefix, "matches: %lu", bf_list_size(&rule->matches));

    DUMP(bf_dump_prefix_last(prefix), "target:");
    bf_dump_prefix_push(prefix);
    DUMP(prefix, "type: %s", bf_target_type_to_str(rule->target->type));
    DUMP(bf_dump_prefix_last(prefix), "verdict: %s",
         bf_target_standard_verdict_to_str(rule->target->verdict));
    bf_dump_prefix_pop(prefix);

    bf_dump_prefix_pop(prefix);
}
