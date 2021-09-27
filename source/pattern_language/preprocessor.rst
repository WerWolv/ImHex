Preprocessor
============

The preprocessor works in a similar fashion to the one found in C/C++.
All lines that start with a ``#`` symbol are treated as preprocessor directives and get evaluated before the syntax of the rest of the program gets analyzed.

``#define``
-----------

.. code-block:: hexpat

    #define MY_CONSTANT 1337

This directive causes a find-and-replace to be performed. 
In the example above, the label ``MY_CONSTANT`` will be replaced with ``1337`` throughout the entire program without doing any sort of lexical analysis.
This means the directive will be replaced even within strings. Additionally, if multiple defines are used, later find-and-replaces can modify 
expressions that got altered by previous ones.

``#include``
------------

.. code-block:: hexpat

    #include <mylibrary.hexpat>

This directive allows inclusion of other files into the current program.
The content of the specified file gets copied directly into the current file.

``#pragma``
-----------

.. code-block:: hexpat

    #pragma endian big

Pragmas are hints to ImHex and the evaluator to tell it how it should treat certain things.

The following pragmas are available:

``endian``
^^^^^^^^^^

**Possible values:** ``big``, ``little``, ``native``

This pragma overwrites the default endianess of all variables declared in the file. By default ``native`` endianess is used.

``MIME``
^^^^^^^^

**Possible values:** Any MIME Type string

This pragma specifies the MIME type of files that can be interpreted by this pattern.
This is useful for automatically loading relevant patterns when a file is opened. The MIME type of the loaded file will be matched against the MIME type specified here and if it matches, a popup will appear asking if this pattern should get loaded.

``base_address``
^^^^^^^^^^^^^^^^

**Possible values:** Any integer value

This pragma automatically adjusts the base address of the currently loaded file.
This is useful for patterns that depend on a file being loaded at a certain address in memory.

``eval_depth``
^^^^^^^^^^^^^^

**Possible values:** Any integer value

This pragma sets the evaluation depth of recursive functions and types.
To prevent ImHex from crashing when evaluating infinitely deep recursive types, ImHex will abort evaluation prematurely if it detects recursion that is too deep. This pragma can adjust the maximum depth allowed

``array_limit``
^^^^^^^^^^^^^^^

**Possible values:** Any integer value

This pragma sets the maximum number of entries allowed in an array.
To prevent ImHex using up a lot of memory when creating huge arrays, ImHex will abort evaluation prematurely if an array with too many entries is evaluated. This pragma can adjust the maximum number of entries allowed