Arrays
======

Arrays are a contiguous collection of one or more value of the same type.

Constant sized array
^^^^^^^^^^^^^^^^^^^^

A contant size can be specified by entering the number of entries in the square brackets. This value may also name another variable which will be read to get the size.

.. code-block:: hexpat

  u32 array[100] @ 0x00;


Unsized array
^^^^^^^^^^^^^

It's possible to leave the size of the array empty in which case it will keep on growing until it hits an entry which is all zeros.

.. code-block:: hexpat

  char string[] @ 0x00;


Loop sized array
^^^^^^^^^^^^^^^^

Sometimes arrays need to keep on growing as long as a certian condition is met. The following array will grow until it hits a byte with the value ``0xFF``.

.. code-block:: hexpat

  u8 string[while(std::mem::read_unsigned($, 1) != 0xFF)] @ 0x00;


Optimized arrays
^^^^^^^^^^^^^^^^

Big arrays take a long time to compute and take up a lot of memory. Because of this, arrays of built-in types are automatically optimized to only create one instance of the
array type and move it around accordingly.

The same optimization can be used for custom types by marking them with the ``[[static]]`` attribute. However this can only be done if the custom type always has the same size and same memory layout. Otherwise results may be invalid!