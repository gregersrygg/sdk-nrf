.. _modem_trace_backend_sample:

nRF9160: Modem trace external flash backend
############################

.. contents::
   :local:
   :depth: 2

The Modem trace external flash backend sample demonstrates how to add a user-defined modem trace backend which stores the trace data to external flash.

Requirements
************

The sample supports the following development kit, version 0.14.0 or higher:

.. table-from-sample-yaml::

.. include:: /includes/tfm.txt

On the nRF9160 DK, set the control signal from the nRF52840 board controller MCU (**P0.19**) to *high* to let the nRF9160 communicate with the external flash memory.
Enable the ``external_flash_pins_routing`` node in devicetree.
See :ref:`zephyr:nrf9160dk_board_controller_firmware` for details on building the firmware for the nRF52840 board controller MCU.
