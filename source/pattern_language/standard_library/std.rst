``std``
=======

| This namespace contains general utility functions.
|

------------------------

``std::print(args...)``
^^^^^^^^^^^^^^^^^^^^^^^

**Prints one or multiple values to the console.**

.. table::
    :align: left

    =========== ==================================================
    Parameter   Description
    =========== ==================================================
    ``args...`` A list of arguments to be concatinated and printed
    =========== ==================================================

.. code-block:: hexpat

    std::print("Hello", " ", "World", " ", 42); // Logs "Hello World 42"

------------------------

``std::assert(condition, message)``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Aborts evaluation and prints an error to the console if ``condition`` evaluates to false**


.. table::
    :align: left

    =============== =========================================================================
    Parameter       Description
    =============== =========================================================================
    ``condition``   Condition that needs to be met
    ``message``     Message to be printed to the console if ``condition`` evaluated to false.
    =============== =========================================================================

------------------------

``std::assert_warn(condition, message)``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Prints a warning to the console if ``condition`` evaluates to false**


.. table::
    :align: left

    =============== =========================================================================
    Parameter       Description
    =============== =========================================================================
    ``condition``   Condition that needs to be met
    ``message``     Message to be printed to the console if ``condition`` evaluated to false.
    =============== =========================================================================