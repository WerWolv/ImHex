.. _Pointer Helpers:

``std::ptr`` :version:`1.10.1`
===============================

| This namespace contains various pointer helper functions
|

------------------------

Functions
---------

``std::ptr::relative_to_pointer(u128 offset) -> u128`` :version:`1.10.1`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**For use with the ``[[pointer_base]]`` attribute. Makes a pointer offset relative to itself instead of to the beginning of the data**

.. table::
    :align: left

    =========== =========================================================
    Parameter   Description
    =========== =========================================================
    ``offset``  Original pointer offset
    ``return``  Offset relative to the current pointer
    =========== =========================================================

------------------------

``std::ptr::relative_to_parent(u128 offset) -> u128`` :version:`1.10.1`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**For use with the ``[[pointer_base]]`` attribute. Makes a pointer offset relative to the pointer's parent instead of to the beginning of the data**

.. table::
    :align: left

    =========== =========================================================
    Parameter   Description
    =========== =========================================================
    ``offset``  Original pointer offset
    ``return``  Offset relative to the pointer's parent
    =========== =========================================================

------------------------

``std::ptr::relative_to_end(u128 offset) -> u128`` :version:`1.10.1`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**For use with the ``[[pointer_base]]`` attribute. Makes a pointer offset relative to the end of the data instead of to the beginning of the data**

.. table::
    :align: left

    =========== =========================================================
    Parameter   Description
    =========== =========================================================
    ``offset``  Original pointer offset
    ``return``  Offset relative to the end of the data
    =========== =========================================================