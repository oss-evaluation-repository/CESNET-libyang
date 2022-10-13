/**
 * @file structure.c
 * @author Michal Vasko <mvasko@cesnet.cz>
 * @brief libyang extension plugin - structure (RFC 8791)
 *
 * Copyright (c) 2022 CESNET, z.s.p.o.
 *
 * This source code is licensed under BSD 3-Clause License (the "License").
 * You may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://opensource.org/licenses/BSD-3-Clause
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "compat.h"
#include "libyang.h"
#include "plugins_exts.h"

struct lysp_ext_instance_structure {
    struct lysp_restr *musts;
    uint16_t flags;
    const char *dsc;
    const char *ref;
    struct lysp_tpdf *typedefs;
    struct lysp_node_grp *groupings;
    struct lysp_node *child;
};

struct lysc_ext_instance_structure {
    struct lysc_must *musts;
    uint16_t flags;
    const char *dsc;
    const char *ref;
    struct lysc_node *child;
};

struct lysp_ext_instance_augment_structure {
    uint16_t flags;
    const char *dsc;
    const char *ref;
    struct lysp_node *child;
};

struct lysc_ext_instance_augment_structure {
    uint16_t flags;
    const char *dsc;
    const char *ref;
};

/**
 * @brief Parse structure extension instances.
 *
 * Implementation of ::lyplg_ext_parse_clb callback set as lyext_plugin::parse.
 */
static LY_ERR
structure_parse(struct lysp_ctx *pctx, struct lysp_ext_instance *ext)
{
    LY_ERR rc;
    LY_ARRAY_COUNT_TYPE u;
    struct lysp_module *pmod;
    struct lysp_ext_instance_structure *struct_pdata;

    /* structure can appear only at the top level of a YANG module or submodule */
    if ((ext->parent_stmt != LY_STMT_MODULE) && (ext->parent_stmt != LY_STMT_SUBMODULE)) {
        lyplg_ext_parse_log(pctx, ext, LY_LLERR, LY_EVALID,
                "Extension %s must not be used as a non top-level statement in \"%s\" statement.", ext->name,
                lyplg_ext_stmt2str(ext->parent_stmt));
        return LY_EVALID;
    }

    pmod = ext->parent;

    /* check for duplication */
    LY_ARRAY_FOR(pmod->exts, u) {
        if ((&pmod->exts[u] != ext) && (pmod->exts[u].name == ext->name) && !strcmp(pmod->exts[u].argument, ext->argument)) {
            /* duplication of the same structure extension in a single module */
            lyplg_ext_parse_log(pctx, ext, LY_LLERR, LY_EVALID, "Extension %s is instantiated multiple times.", ext->name);
            return LY_EVALID;
        }
    }

    /* allocate the storage */
    struct_pdata = calloc(1, sizeof *struct_pdata);
    if (!struct_pdata) {
        goto emem;
    }
    ext->parsed = struct_pdata;
    LY_ARRAY_CREATE_GOTO(lyplg_extp_cur_pmod(pctx)->mod->ctx, ext->substmts, 14, rc, emem);

    /* parse substatements */
    LY_ARRAY_INCREMENT(ext->substmts);
    ext->substmts[0].stmt = LY_STMT_MUST;
    ext->substmts[0].storage = &struct_pdata->musts;

    LY_ARRAY_INCREMENT(ext->substmts);
    ext->substmts[1].stmt = LY_STMT_STATUS;
    ext->substmts[1].storage = &struct_pdata->flags;

    LY_ARRAY_INCREMENT(ext->substmts);
    ext->substmts[2].stmt = LY_STMT_DESCRIPTION;
    ext->substmts[2].storage = &struct_pdata->dsc;

    LY_ARRAY_INCREMENT(ext->substmts);
    ext->substmts[3].stmt = LY_STMT_REFERENCE;
    ext->substmts[3].storage = &struct_pdata->ref;

    LY_ARRAY_INCREMENT(ext->substmts);
    ext->substmts[4].stmt = LY_STMT_TYPEDEF;
    ext->substmts[4].storage = &struct_pdata->typedefs;

    LY_ARRAY_INCREMENT(ext->substmts);
    ext->substmts[5].stmt = LY_STMT_GROUPING;
    ext->substmts[5].storage = &struct_pdata->groupings;

    /* data-def-stmt */
    LY_ARRAY_INCREMENT(ext->substmts);
    ext->substmts[6].stmt = LY_STMT_CONTAINER;
    ext->substmts[6].storage = &struct_pdata->child;

    LY_ARRAY_INCREMENT(ext->substmts);
    ext->substmts[7].stmt = LY_STMT_LEAF;
    ext->substmts[7].storage = &struct_pdata->child;

    LY_ARRAY_INCREMENT(ext->substmts);
    ext->substmts[8].stmt = LY_STMT_LEAF_LIST;
    ext->substmts[8].storage = &struct_pdata->child;

    LY_ARRAY_INCREMENT(ext->substmts);
    ext->substmts[9].stmt = LY_STMT_LIST;
    ext->substmts[9].storage = &struct_pdata->child;

    LY_ARRAY_INCREMENT(ext->substmts);
    ext->substmts[10].stmt = LY_STMT_CHOICE;
    ext->substmts[10].storage = &struct_pdata->child;

    LY_ARRAY_INCREMENT(ext->substmts);
    ext->substmts[11].stmt = LY_STMT_ANYDATA;
    ext->substmts[11].storage = &struct_pdata->child;

    LY_ARRAY_INCREMENT(ext->substmts);
    ext->substmts[12].stmt = LY_STMT_ANYXML;
    ext->substmts[12].storage = &struct_pdata->child;

    LY_ARRAY_INCREMENT(ext->substmts);
    ext->substmts[13].stmt = LY_STMT_USES;
    ext->substmts[13].storage = &struct_pdata->child;

    rc = lyplg_ext_parse_extension_instance(pctx, ext);
    return rc;

emem:
    lyplg_ext_parse_log(pctx, ext, LY_LLERR, LY_EMEM, "Memory allocation failed (%s()).", __func__);
    return LY_EMEM;
}

/**
 * @brief Compile structure extension instances.
 *
 * Implementation of ::lyplg_ext_compile_clb callback set as lyext_plugin::compile.
 */
static LY_ERR
structure_compile(struct lysc_ctx *cctx, const struct lysp_ext_instance *extp, struct lysc_ext_instance *ext)
{
    LY_ERR rc;
    struct lysc_module *mod_c;
    const struct lysc_node *child;
    struct lysc_ext_instance_structure *struct_cdata;
    uint32_t prev_options = *lyplg_ext_compile_get_options(cctx);

    mod_c = ext->parent;

    /* check identifier namespace with the compiled nodes */
    LY_LIST_FOR(mod_c->data, child) {
        if (!strcmp(child->name, ext->argument)) {
            /* identifier collision */
            lyplg_ext_compile_log(cctx, ext, LY_LLERR, LY_EVALID,  "Extension %s collides with a %s with the same identifier.",
                    extp->name, lys_nodetype2str(child->nodetype));
            return LY_EVALID;
        }
    }

    /* allocate the storage */
    struct_cdata = calloc(1, sizeof *struct_cdata);
    if (!struct_cdata) {
        goto emem;
    }
    ext->compiled = struct_cdata;

    /* compile substatements */
    LY_ARRAY_CREATE_GOTO(cctx->ctx, ext->substmts, 14, rc, emem);
    LY_ARRAY_INCREMENT(ext->substmts);
    ext->substmts[0].stmt = LY_STMT_MUST;
    ext->substmts[0].storage = &struct_cdata->musts;

    LY_ARRAY_INCREMENT(ext->substmts);
    ext->substmts[1].stmt = LY_STMT_STATUS;
    ext->substmts[1].storage = &struct_cdata->flags;

    LY_ARRAY_INCREMENT(ext->substmts);
    ext->substmts[2].stmt = LY_STMT_DESCRIPTION;
    ext->substmts[2].storage = &struct_cdata->dsc;

    LY_ARRAY_INCREMENT(ext->substmts);
    ext->substmts[3].stmt = LY_STMT_REFERENCE;
    ext->substmts[3].storage = &struct_cdata->ref;

    LY_ARRAY_INCREMENT(ext->substmts);
    ext->substmts[4].stmt = LY_STMT_TYPEDEF;
    ext->substmts[4].storage = NULL;

    LY_ARRAY_INCREMENT(ext->substmts);
    ext->substmts[5].stmt = LY_STMT_GROUPING;
    ext->substmts[5].storage = NULL;

    /* data-def-stmt */
    LY_ARRAY_INCREMENT(ext->substmts);
    ext->substmts[6].stmt = LY_STMT_CONTAINER;
    ext->substmts[6].storage = &struct_cdata->child;

    LY_ARRAY_INCREMENT(ext->substmts);
    ext->substmts[7].stmt = LY_STMT_LEAF;
    ext->substmts[7].storage = &struct_cdata->child;

    LY_ARRAY_INCREMENT(ext->substmts);
    ext->substmts[8].stmt = LY_STMT_LEAF_LIST;
    ext->substmts[8].storage = &struct_cdata->child;

    LY_ARRAY_INCREMENT(ext->substmts);
    ext->substmts[9].stmt = LY_STMT_LIST;
    ext->substmts[9].storage = &struct_cdata->child;

    LY_ARRAY_INCREMENT(ext->substmts);
    ext->substmts[10].stmt = LY_STMT_CHOICE;
    ext->substmts[10].storage = &struct_cdata->child;

    LY_ARRAY_INCREMENT(ext->substmts);
    ext->substmts[11].stmt = LY_STMT_ANYDATA;
    ext->substmts[11].storage = &struct_cdata->child;

    LY_ARRAY_INCREMENT(ext->substmts);
    ext->substmts[12].stmt = LY_STMT_ANYXML;
    ext->substmts[12].storage = &struct_cdata->child;

    LY_ARRAY_INCREMENT(ext->substmts);
    ext->substmts[13].stmt = LY_STMT_USES;
    ext->substmts[13].storage = &struct_cdata->child;

    *lyplg_ext_compile_get_options(cctx) |= LYS_COMPILE_NO_CONFIG | LYS_COMPILE_NO_DISABLED;
    rc = lyplg_ext_compile_extension_instance(cctx, extp, ext);
    *lyplg_ext_compile_get_options(cctx) = prev_options;
    if (rc) {
        return rc;
    }

    return LY_SUCCESS;

emem:
    lyplg_ext_compile_log(cctx, ext, LY_LLERR, LY_EMEM, "Memory allocation failed (%s()).", __func__);
    return LY_EMEM;
}

/**
 * @brief INFO printer
 *
 * Implementation of ::lyplg_ext_schema_printer_clb set as ::lyext_plugin::sprinter
 */
static LY_ERR
structure_sprinter(struct lyspr_ctx *ctx, struct lysc_ext_instance *ext, ly_bool *flag)
{
    lyplg_ext_print_extension_instance(ctx, ext, flag);
    return LY_SUCCESS;
}

/**
 * @brief Free parsed structure extension instance data.
 *
 * Implementation of ::lyplg_clb_parse_free_clb callback set as lyext_plugin::pfree.
 */
static void
structure_pfree(const struct ly_ctx *ctx, struct lysp_ext_instance *ext)
{
    lyplg_ext_pfree_instance_substatements(ctx, ext->substmts);
    free(ext->parsed);
}

/**
 * @brief Free compiled structure extension instance data.
 *
 * Implementation of ::lyplg_clb_compile_free_clb callback set as lyext_plugin::cfree.
 */
static void
structure_cfree(const struct ly_ctx *ctx, struct lysc_ext_instance *ext)
{
    lyplg_ext_cfree_instance_substatements(ctx, ext->substmts);
    free(ext->compiled);
}

/**
 * @brief Parse augment-structure extension instances.
 *
 * Implementation of ::lyplg_ext_parse_clb callback set as lyext_plugin::parse.
 */
static LY_ERR
structure_aug_parse(struct lysp_ctx *pctx, struct lysp_ext_instance *ext)
{
    LY_ERR rc;
    struct lysp_stmt *stmt;
    struct lysp_ext_instance_augment_structure *aug_pdata;

    /* augment-structure can appear only at the top level of a YANG module or submodule */
    if ((ext->parent_stmt != LY_STMT_MODULE) && (ext->parent_stmt != LY_STMT_SUBMODULE)) {
        lyplg_ext_parse_log(pctx, ext, LY_LLERR, LY_EVALID,
                "Extension %s must not be used as a non top-level statement in \"%s\" statement.", ext->name,
                lyplg_ext_stmt2str(ext->parent_stmt));
        return LY_EVALID;
    }

    /* augment-structure must define some data-def-stmt */
    LY_LIST_FOR(ext->child, stmt) {
        if (stmt->kw & LY_STMT_DATA_NODE_MASK) {
            break;
        }
    }
    if (!stmt) {
        lyplg_ext_parse_log(pctx, ext, LY_LLERR, LY_EVALID, "Extension %s does not define any data-def-stmt statements.",
                ext->name);
        return LY_EVALID;
    }

    /* allocate the storage */
    aug_pdata = calloc(1, sizeof *aug_pdata);
    if (!aug_pdata) {
        goto emem;
    }
    ext->parsed = aug_pdata;
    LY_ARRAY_CREATE_GOTO(lyplg_extp_cur_pmod(pctx)->mod->ctx, ext->substmts, 12, rc, emem);

    /* parse substatements */
    LY_ARRAY_INCREMENT(ext->substmts);
    ext->substmts[0].stmt = LY_STMT_STATUS;
    ext->substmts[0].storage = &aug_pdata->flags;

    LY_ARRAY_INCREMENT(ext->substmts);
    ext->substmts[1].stmt = LY_STMT_DESCRIPTION;
    ext->substmts[1].storage = &aug_pdata->dsc;

    LY_ARRAY_INCREMENT(ext->substmts);
    ext->substmts[2].stmt = LY_STMT_REFERENCE;
    ext->substmts[2].storage = &aug_pdata->ref;

    /* data-def-stmt */
    LY_ARRAY_INCREMENT(ext->substmts);
    ext->substmts[3].stmt = LY_STMT_CONTAINER;
    ext->substmts[3].storage = &aug_pdata->child;

    LY_ARRAY_INCREMENT(ext->substmts);
    ext->substmts[4].stmt = LY_STMT_LEAF;
    ext->substmts[4].storage = &aug_pdata->child;

    LY_ARRAY_INCREMENT(ext->substmts);
    ext->substmts[5].stmt = LY_STMT_LEAF_LIST;
    ext->substmts[5].storage = &aug_pdata->child;

    LY_ARRAY_INCREMENT(ext->substmts);
    ext->substmts[6].stmt = LY_STMT_LIST;
    ext->substmts[6].storage = &aug_pdata->child;

    LY_ARRAY_INCREMENT(ext->substmts);
    ext->substmts[7].stmt = LY_STMT_CHOICE;
    ext->substmts[7].storage = &aug_pdata->child;

    LY_ARRAY_INCREMENT(ext->substmts);
    ext->substmts[8].stmt = LY_STMT_ANYDATA;
    ext->substmts[8].storage = &aug_pdata->child;

    LY_ARRAY_INCREMENT(ext->substmts);
    ext->substmts[9].stmt = LY_STMT_ANYXML;
    ext->substmts[9].storage = &aug_pdata->child;

    LY_ARRAY_INCREMENT(ext->substmts);
    ext->substmts[10].stmt = LY_STMT_USES;
    ext->substmts[10].storage = &aug_pdata->child;

    /* case */
    LY_ARRAY_INCREMENT(ext->substmts);
    ext->substmts[11].stmt = LY_STMT_CASE;
    ext->substmts[11].storage = &aug_pdata->child;

    rc = lyplg_ext_parse_extension_instance(pctx, ext);
    return rc;

emem:
    lyplg_ext_parse_log(pctx, ext, LY_LLERR, LY_EMEM, "Memory allocation failed (%s()).", __func__);
    return LY_EMEM;
}

/**
 * @brief Compile augment-structure extension instances.
 *
 * Implementation of ::lyplg_ext_compile_clb callback set as lyext_plugin::compile.
 */
static LY_ERR
structure_aug_compile(struct lysc_ctx *cctx, const struct lysp_ext_instance *extp, struct lysc_ext_instance *ext)
{
    LY_ERR rc;
    struct lysc_node *aug_target;
    struct lysc_ext_instance *target_ext;
    struct lysc_ext_instance_structure *target_cdata;
    struct lysc_ext_instance_augment_structure *aug_cdata;
    uint32_t prev_options = *lyplg_ext_compile_get_options(cctx), i;

    /* find the target struct ext instance */
    if ((rc = lys_compile_extension_instance_find_augment_target(cctx, extp->argument, &target_ext, &aug_target))) {
        return rc;
    }

    /* check target_ext */
    if (strcmp(target_ext->def->name, "structure") || strcmp(target_ext->def->module->name, "ietf-yang-structure-ext")) {
        lyplg_ext_compile_log(cctx, ext, LY_LLERR, LY_EVALID,
                "Extension %s can only target extension instances of \"ietf-yang-structure-ext:structure\".", extp->name);
        return LY_EVALID;
    }
    target_cdata = target_ext->compiled;

    /* allocate the storage */
    aug_cdata = calloc(1, sizeof *aug_cdata);
    if (!aug_cdata) {
        goto emem;
    }
    ext->compiled = aug_cdata;

    /* compile substatements */
    LY_ARRAY_CREATE_GOTO(cctx->ctx, ext->substmts, 12, rc, emem);
    LY_ARRAY_INCREMENT(ext->substmts);
    ext->substmts[0].stmt = LY_STMT_STATUS;
    ext->substmts[0].storage = &aug_cdata->flags;

    LY_ARRAY_INCREMENT(ext->substmts);
    ext->substmts[1].stmt = LY_STMT_DESCRIPTION;
    ext->substmts[1].storage = &aug_cdata->dsc;

    LY_ARRAY_INCREMENT(ext->substmts);
    ext->substmts[2].stmt = LY_STMT_REFERENCE;
    ext->substmts[2].storage = &aug_cdata->ref;

    /* data-def-stmt */
    LY_ARRAY_INCREMENT(ext->substmts);
    ext->substmts[3].stmt = LY_STMT_CONTAINER;
    ext->substmts[3].storage = &target_cdata->child;

    LY_ARRAY_INCREMENT(ext->substmts);
    ext->substmts[4].stmt = LY_STMT_LEAF;
    ext->substmts[4].storage = &target_cdata->child;

    LY_ARRAY_INCREMENT(ext->substmts);
    ext->substmts[5].stmt = LY_STMT_LEAF_LIST;
    ext->substmts[5].storage = &target_cdata->child;

    LY_ARRAY_INCREMENT(ext->substmts);
    ext->substmts[6].stmt = LY_STMT_LIST;
    ext->substmts[6].storage = &target_cdata->child;

    LY_ARRAY_INCREMENT(ext->substmts);
    ext->substmts[7].stmt = LY_STMT_CHOICE;
    ext->substmts[7].storage = &target_cdata->child;

    LY_ARRAY_INCREMENT(ext->substmts);
    ext->substmts[8].stmt = LY_STMT_ANYDATA;
    ext->substmts[8].storage = &target_cdata->child;

    LY_ARRAY_INCREMENT(ext->substmts);
    ext->substmts[9].stmt = LY_STMT_ANYXML;
    ext->substmts[9].storage = &target_cdata->child;

    LY_ARRAY_INCREMENT(ext->substmts);
    ext->substmts[10].stmt = LY_STMT_USES;
    ext->substmts[10].storage = &target_cdata->child;

    /* case */
    LY_ARRAY_INCREMENT(ext->substmts);
    ext->substmts[11].stmt = LY_STMT_CASE;
    ext->substmts[11].storage = &target_cdata->child;

    *lyplg_ext_compile_get_options(cctx) |= LYS_COMPILE_NO_CONFIG | LYS_COMPILE_NO_DISABLED;
    rc = lyplg_ext_compile_extension_instance_augment(cctx, extp, ext, aug_target);
    *lyplg_ext_compile_get_options(cctx) = prev_options;
    if (rc) {
        return rc;
    }

    /* data-def-statements are now part of the target extension (do not print nor free them) */
    for (i = 0; i < 9; ++i) {
        LY_ARRAY_DECREMENT(ext->substmts);
    }

    return LY_SUCCESS;

emem:
    lyplg_ext_compile_log(cctx, ext, LY_LLERR, LY_EMEM, "Memory allocation failed (%s()).", __func__);
    return LY_EMEM;
}

/**
 * @brief Plugin descriptions for the structure extension
 *
 * Note that external plugins are supposed to use:
 *
 *   LYPLG_EXTENSIONS = {
 */
const struct lyplg_ext_record plugins_structure[] = {
    {
        .module = "ietf-yang-structure-ext",
        .revision = "2020-06-17",
        .name = "structure",

        .plugin.id = "ly2 structure v1",
        .plugin.parse = structure_parse,
        .plugin.compile = structure_compile,
        .plugin.sprinter = structure_sprinter,
        .plugin.node = NULL,
        .plugin.snode = NULL,
        .plugin.validate = NULL,
        .plugin.pfree = structure_pfree,
        .plugin.cfree = structure_cfree
    },
    {
        .module = "ietf-yang-structure-ext",
        .revision = "2020-06-17",
        .name = "augment-structure",

        .plugin.id = "ly2 structure v1",
        .plugin.parse = structure_aug_parse,
        .plugin.compile = structure_aug_compile,
        .plugin.sprinter = structure_sprinter,
        .plugin.node = NULL,
        .plugin.snode = NULL,
        .plugin.validate = NULL,
        .plugin.pfree = NULL,
        .plugin.cfree = structure_cfree
    },
    {0}     /* terminating zeroed record */
};