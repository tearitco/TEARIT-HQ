/* nb_worker_install_parser_serializer.c - DOMParser/XMLSerializer global installs (mechanical extract, single-TU include) */
        /* DOMParser: real HTML/XML snippet parsing into a document wrapper. */
        f = add_class(ctx, "DOMParser", JS_UNDEFINED);
        p = JS_GetPropertyStr(ctx, f, "prototype");
        JS_SetPropertyStr(ctx, p, "parseFromString", JS_NewCFunction(ctx, nb_domparser_parse, "parseFromString", 2));
        JS_FreeValue(ctx, p);
        JS_SetPropertyStr(ctx, g, "DOMParser", f);
        /* XMLSerializer: real outer-HTML serialization. */
        f = add_class(ctx, "XMLSerializer", JS_UNDEFINED);
        p = JS_GetPropertyStr(ctx, f, "prototype");
        JS_SetPropertyStr(ctx, p, "serializeToString", JS_NewCFunction(ctx, nb_xmlserializer_serialize, "serializeToString", 1));
        JS_FreeValue(ctx, p);
        JS_SetPropertyStr(ctx, g, "XMLSerializer", f);
