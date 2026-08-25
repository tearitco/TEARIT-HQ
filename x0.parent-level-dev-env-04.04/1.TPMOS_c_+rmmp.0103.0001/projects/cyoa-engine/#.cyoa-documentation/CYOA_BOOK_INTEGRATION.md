# CYOA Book Integration Guide

This guide outlines the process for integrating new Choose Your Own Adventure (CYOA) books into the CHTPM+OS CYOA engine.

## 1. CYOA Book Structure and File Naming

CYOA books are composed of individual text files, each representing a page.

*   **Book Directory:** Place your CYOA book content within a dedicated directory. This could be a new folder under `more-cyoa/` or within the existing `^.CYOA_DATA_XL_69/books/` structure (e.g., `book_4/`, `book_5/`).
*   **Page Files:** Individual pages within a book should follow a consistent naming convention. The recommended format is:
    `page_NN.txt`
    where `NN` is a zero-padded two-digit number. For example:
    *   `page_01.txt`
    *   `page_02.txt`
    *   ...
    *   `page_10.txt`
    *   `page_11.txt`
    This convention is used by the `renumber_cyoa.py` scripts for managing page sequences.

## 2. Page Navigation and Linking

The CYOA engine navigates between pages based on links embedded within the text of each page.

*   **Link Format:** Choices that lead to another page must explicitly state the target page number using the following convention:
    `Turn to page X.`
    or
    `turn to page X.`
    where `X` is the numerical identifier of the target page (e.g., `Turn to page 5.`).
*   **Regular Expression:** The engine likely uses a pattern similar to `((?:turn to|Turn to) page )(\d+)(\.?)` to parse these links.
*   **Example:** A choice in `page_04.txt` might read:
    `* If you decide to investigate the shimmering pool, turn to page 24.`

## 3. Pitfalls and Best Practices

*   **Missing Pages:** The most common issue is a link to a page that does not exist. If `page_15.txt` is referenced but is missing, the engine will likely fail to load that page, potentially causing an error, crash, or a blank screen. **Always ensure that every page number referenced in a link corresponds to an existing page file.**
*   **Consistency:** Maintain consistent formatting and numbering throughout the book to ensure smooth navigation.
*   **Renumbering Utility:** The `renumber_cyoa.py` scripts (found in `more-cyoa/*.sp.maze-cyoa/` and `more-cyoa/2.ngl-cyoa-1/`) can be used to help manage and renumber pages. Familiarize yourself with their usage if you plan to make significant structural changes to a book.

## 4. TPMOS Platform Documentation

Documentation for the TPMOS platform itself should be placed within the `1.TPMOS_c_+rmmp_69.11+joy/#.docs/` directory.

## 5. New Books

To add new CYOA books, you can indeed drop them into the
  ^.CYOA_DATA_XL_69/books/ directory or create a new folder under
  more-cyoa/ (e.g., more-cyoa/book_5/).

  As detailed in the updated CYOA_BOOK_INTEGRATION.md file located at
  @cyoa-documentation/CYOA_BOOK_INTEGRATION.md, ensure that:
   * Individual pages within your book are named using the page_NN.txt
     convention (e.g., page_01.txt, page_02.txt).
   * All "Turn to page X" links within the book correctly reference
     existing page files. The engine relies on these explicit links for
     navigation.

  Following these guidelines should allow the CYOA engine to integrate
  and read your new books seamlessly.

## 6. Non Cyoa Books. 
Should include a footer for changing page ; + optional .mp3 in pages directory in the same format as the clone books

