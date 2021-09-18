Functions
=========

Functions are reusable pieces of code that can do calculations. Pretty much like functions in any other programming language.

Parameter types need to be specified explicitly, return type is automatically deduced.

.. code-block:: hexpat

    fn min(s32 a, s32 b) {
        if (a > b)
            return b;
        else
            return a;
    };

    std::print(min(100, 200)); // 100

Variables
^^^^^^^^^

Variables can be declared in a similar fashion as outside of functions but they will be put onto the function's stack instead of being highlighted.


.. code-block:: hexpat

    fn get_value() {
        u32 value;

        value = 1234;

        return value;
    };



Control statements
^^^^^^^^^^^^^^^^^^

If-else blocks can be used the same as outside of functions to cause code to only execute when the condition is met.

.. code-block:: hexpat

    if (x > 5) {
        // Execute when x is greater than 5
    } else {
        // Execute otherwise
    }

While loops can be used as well.

.. code-block:: hexpat

    u32 i;
    while (i < 100) {
        // Do calculation
        i = i + 1;
    }


Return statements
^^^^^^^^^^^^^^^^^

In order to return a value from a function, the ``return`` keyword is used.

The return type of the function will automatically be determined by the value returned.

.. code-block:: hexpat

    fn get_value() {
        return 1234;
    };

    std::print(get_value()); // 1234