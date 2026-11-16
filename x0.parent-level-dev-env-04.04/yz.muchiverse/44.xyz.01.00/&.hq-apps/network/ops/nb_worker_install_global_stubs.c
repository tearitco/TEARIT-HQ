/* nb_worker_install_global_stubs.c - standard element/event constructor global stubs (mechanical extract, single-TU include) */
    /* standard element/event constructor globals: bundles load-time-read
     * X.prototype (Object.create / extends) and branch on instanceof for a
     * long list of names. All inert; prototypes chain onto the hierarchy. */
    {
        static const char *html_els[] = {
            "HTMLTemplateElement", "HTMLUnknownElement", "HTMLScriptElement",
            "HTMLStyleElement", "HTMLDivElement", "HTMLSpanElement",
            "HTMLAnchorElement", "HTMLImageElement", "HTMLInputElement",
            "HTMLButtonElement", "HTMLFormElement", "HTMLLinkElement",
            "HTMLMetaElement", "HTMLHeadElement", "HTMLBodyElement",
            "HTMLHtmlElement", "HTMLCanvasElement", "HTMLVideoElement",
            "HTMLAudioElement", "HTMLMediaElement", "HTMLIFrameElement",
            "HTMLSelectElement", "HTMLOptionElement", "HTMLTextAreaElement",
            "HTMLLabelElement", "HTMLUListElement", "HTMLLIElement",
            "HTMLTableElement", "HTMLParagraphElement", "HTMLHeadingElement",
            "HTMLTitleElement", "HTMLSlotElement", "HTMLPictureElement",
            "HTMLSourceElement", "HTMLTrackElement", "HTMLBRElement",
            "HTMLHRElement", "HTMLPreElement", "HTMLOListElement",
            "HTMLDListElement", "HTMLMenuElement", "HTMLDialogElement"
        };
        for (size_t i = 0; i < sizeof(html_els) / sizeof(html_els[0]); i++)
            add_global_class(ctx, g, html_els[i], Hp);
        static const char *svg_els[] = {
            "SVGSVGElement", "SVGGraphicsElement", "SVGGeometryElement",
            "SVGPathElement", "SVGUseElement", "SVGImageElement",
            "SVGTextElement", "SVGGElement", "SVGRectElement",
            "SVGCircleElement", "SVGDefsElement", "SVGSymbolElement"
        };
        for (size_t i = 0; i < sizeof(svg_els) / sizeof(svg_els[0]); i++)
            add_global_class(ctx, g, svg_els[i], Ep);
        static const char *node_els[] = {
            "DocumentFragment", "ShadowRoot", "CharacterData", "Comment",
            "ProcessingInstruction", "DocumentType", "Attr", "CDATASection"
        };
        for (size_t i = 0; i < sizeof(node_els) / sizeof(node_els[0]); i++)
            add_global_class(ctx, g, node_els[i], Np);
        static const char *plain[] = {
            "NodeList", "HTMLCollection", "DOMTokenList", "NamedNodeMap",
            "MutationObserver", "MutationRecord", "CustomElementRegistry",
            "DOMParser", "XMLSerializer", "Range", "Selection",
            "Event", "CustomEvent", "MouseEvent", "KeyboardEvent",
            "PointerEvent", "TouchEvent", "FocusEvent", "InputEvent",
            "WheelEvent", "UIEvent", "ProgressEvent", "ErrorEvent",
            "MessageEvent", "DragEvent", "AnimationEvent", "TransitionEvent",
            "File", "URLSearchParams",
            "IntersectionObserver", "ResizeObserver", "PerformanceObserver"
        };
        for (size_t i = 0; i < sizeof(plain) / sizeof(plain[0]); i++)
            add_global_class(ctx, g, plain[i], JS_UNDEFINED);
