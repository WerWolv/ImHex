Pointers
========

Pointers are variables that treat their value as an address to find the address of the value they are pointing to.

.. code-block:: hexpat

    u16 *pointer : u32 @ 0x08;

This code declares a pointer whose address is a ``u32`` and points to a ``u16``.

The address will always be treated as absolute. Make sure to set the base address of your data correctly in order for pointers to work as intended.

.. image:: assets/pointers/hex.png
  :width: 100%
  :alt: Pointer Highlighing

.. image:: assets/pointers/data.png
  :width: 100%
  :alt: Pointer Decoding