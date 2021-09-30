``std::http`` :version:`1.10.1`
================================

| This namespace contains networking functions
|

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