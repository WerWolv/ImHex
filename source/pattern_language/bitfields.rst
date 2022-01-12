Bitfields
=========

Bitfields are similar to structs but they address individual, unaligned bits instead. 
They can be used to decode bit flags or other types that use less than 8 bits to store a value.

.. code-block:: hexpat

  bitfield Permission {
    r : 1;
    w : 1;
    x : 1;
  };

Each entry inside of a bitfield consists of a field name followed by a colon and the size of the field in bits.
A single field cannot occupy more than 64 bits.

.. image:: assets/bitfields/data.png
  :width: 100%
  :alt: Bitfields Decoding

Padding :version:`1.12.0`
^^^^^^^^^^^^^^^^^^^^^^^^^^

It's also possible to insert padding inbetween fields using the padding syntax.

.. code-block:: hexpat

  bitfield Flags {
    a : 1;
    b : 2;
    padding : 4;
    c : 1;
  };

This inserts a 4 bit padding between field ``b`` and ``c``.