``Networking Library`` :version:`1.10.1`
========================================

.. code-block:: hexpat

    #include <std/http.pat>

| This namespace contains networking functions
|

.. warning::

    These functions are considered dangerous and require the user to confirm that they really want to run this pattern.

------------------------

Functions
---------

``std::http::get(str url) -> str`` :version:`1.10.1`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Issues a ``GET`` request to the passed URL and returns the received data as a string**

.. table::
    :align: left

    =========== =========================================================
    Parameter   Description
    =========== =========================================================
    ``url``     URL to make request to
    ``return``  Response body
    =========== =========================================================