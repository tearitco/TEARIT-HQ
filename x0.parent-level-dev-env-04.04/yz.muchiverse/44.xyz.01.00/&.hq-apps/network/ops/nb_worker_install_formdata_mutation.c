/* nb_worker_install_formdata_mutation.c - FormData/Blob/FileReader and MutationObserver installs (mechanical extract, single-TU include) */
        /* FormData / Blob / FileReader carry real state, so they are built
         * explicitly rather than by the inert-stub loop above. */
        nb_install_formdata_blob(ctx, g);
        /* MutationObserver: real child-list/attribute callback slice. */
        JSValue f = JS_NewCFunction2(ctx, nb_mutation_observer_ctor, "MutationObserver", 1, JS_CFUNC_constructor, 0);
        JSValue p = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, f, "prototype", p);
        JS_SetPropertyStr(ctx, p, "observe", JS_NewCFunction(ctx, nb_mutation_observe, "observe", 2));
        JS_SetPropertyStr(ctx, p, "disconnect", JS_NewCFunction(ctx, nb_mutation_disconnect, "disconnect", 0));
        JS_SetPropertyStr(ctx, p, "takeRecords", JS_NewCFunction(ctx, nb_mutation_take_records, "takeRecords", 0));
        JS_SetPropertyStr(ctx, g, "MutationObserver", f);
