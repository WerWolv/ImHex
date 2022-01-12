Structs
=======

Structs are data types that bundle multiple variables together into one single type.

A very simple struct for a 3D vector of floats might look like this:

.. code-block:: hexpat

  struct Vector3f {
    float x, y, z;
  };

Placing it into memory using the placement syntax will place all members of the struct directly adjacent to each other starting at the specified address.

.. image:: assets/structs/hex.png
  :width: 100%
  :alt: Struct Highlighing

.. image:: assets/structs/data.png
  :width: 100%
  :alt: Struct Decoding

Padding
^^^^^^^

By default there's no padding between struct members. This is not always desired so padding can be inserted manually if needed using the ``padding`` keyword.

.. code-block:: hexpat

  struct Vector3f {
    float x;
    padding[4];
    float y;
    padding[8];
    float z;
  };

This code will insert a 4 byte padding between the members ``x`` and ``y`` as well as a 8 byte padding between ``y`` and ``z``.
  
.. image:: assets/structs/padding.png
  :width: 100%
  :alt: Decoding

Inheritance :version:`1.10.1`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Inheritance allows copying all members of the parent struct into the child struct and make them available there.

.. code-block:: hexpat

  struct Parent {
    u32 type;
    float value;
  };

  struct Child : Parent {
    char string[];
  };

The struct ``Child`` now contains ``type``, ``value`` and ``string``.


Control flow statements :version:`1.13.0`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Control flow statements such as ``break`` or ``continue`` can be used inside of structs and unions and are applied
when this type is used in a array pattern. Outside of arrays, these statements are ignored.


Break :version:`1.13.0`
"""""""""""""""""""""""

When a break is reached, the current array creation process is terminated. 
This means, the array keeps all entries that have currently, including the one that's being currently processed, but won't expand further, even if the requested number of entries hasn't been reached yet.

.. code-block:: hexpat

  struct Test {
    u32 x;

    if (x == 0x11223344)
      break;
  };

  // This array requests 1000 entries but stops growing as soon as it hits a u32 with the value 0x11223344
  // causing it to have a size less than 1000
  Test tests[1000] @ 0x00;


Continue :version:`1.13.0`
""""""""""""""""""""""""""

When a continue is reached, the currently evaluated array entry gets evaluated to find next array entry offset but then gets discarded. 
This can be used to conditionally exclude certain array entries from the list that are either invalid or shouldn't be displayed in the pattern data list
while still scanning the entire range the array would span.


.. code-block:: hexpat

  struct Test {
    u32 value;

    if (value == 0x11223344)
      continue;
  };

  // This array requests 1000 entries but skips all entries where x has the value 0x11223344
  // causing it to have a size less than 1000
  Test tests[1000] @ 0x00;
