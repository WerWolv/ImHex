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