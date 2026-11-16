/* nb_worker_install_observers.c - Intersection/Resize/Performance observer installs (mechanical extract, single-TU include) */
        /* IntersectionObserver: microtask initial + DOM-driven change entries. */
        f = JS_NewCFunction2(ctx, nb_intersection_observer_ctor, "IntersectionObserver", 1, JS_CFUNC_constructor, 0);
        p = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, f, "prototype", p);
        JS_SetPropertyStr(ctx, p, "observe", JS_NewCFunction(ctx, nb_intersection_observe, "observe", 2));
        JS_SetPropertyStr(ctx, p, "unobserve", JS_NewCFunction(ctx, nb_intersection_unobserve, "unobserve", 1));
        JS_SetPropertyStr(ctx, p, "disconnect", JS_NewCFunction(ctx, nb_intersection_disconnect, "disconnect", 0));
        JS_SetPropertyStr(ctx, p, "takeRecords", JS_NewCFunction(ctx, nb_intersection_take_records, "takeRecords", 0));
        JS_SetPropertyStr(ctx, g, "IntersectionObserver", f);
        /* ResizeObserver: microtask initial + DOM-driven change entries. */
        f = JS_NewCFunction2(ctx, nb_resize_observer_ctor, "ResizeObserver", 1, JS_CFUNC_constructor, 0);
        p = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, f, "prototype", p);
        JS_SetPropertyStr(ctx, p, "observe", JS_NewCFunction(ctx, nb_resize_observe, "observe", 2));
        JS_SetPropertyStr(ctx, p, "unobserve", JS_NewCFunction(ctx, nb_resize_unobserve, "unobserve", 1));
        JS_SetPropertyStr(ctx, p, "disconnect", JS_NewCFunction(ctx, nb_resize_disconnect, "disconnect", 0));
        JS_SetPropertyStr(ctx, p, "takeRecords", JS_NewCFunction(ctx, nb_resize_take_records, "takeRecords", 0));
        JS_SetPropertyStr(ctx, g, "ResizeObserver", f);
        /* PerformanceObserver: mark delivery. */
        f = JS_NewCFunction2(ctx, nb_po_ctor, "PerformanceObserver", 1, JS_CFUNC_constructor, 0);
        p = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, f, "prototype", p);
        JS_SetPropertyStr(ctx, p, "observe", JS_NewCFunction(ctx, nb_po_observe, "observe", 1));
        JS_SetPropertyStr(ctx, p, "disconnect", JS_NewCFunction(ctx, nb_po_disconnect, "disconnect", 0));
        JS_SetPropertyStr(ctx, p, "takeRecords", JS_NewCFunction(ctx, nb_po_take_records, "takeRecords", 0));
        JS_SetPropertyStr(ctx, g, "PerformanceObserver", f);
