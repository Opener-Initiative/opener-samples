Opener dectnrp driver
#####################

Overview
********

This sample shows how to include the Opener NR+ stack as an out-of-tree Zephyr module 
in your project and start using the dectnrp driver API directly without upper l2 layer.
This sample specifically demonstrates transmission and reception.

The main flow of this app is as follows:

.. code-block::

    Boot  
     ↓  
    Init  
     ↓  
    Get net-time  
     ↓  
    RSSI1 scan  
     ↓  
    receive  
     ↓   ↑  
    transmit  

In initialization phase it provides the network-id to the driver and initializes the local device.
The long-id is created from chip id and is unique as long as we only use Nordic chips.
The short-id is randomly created. The remote device is not known yet at that stage.

After initialization it reads the network time from modem and does a RSSI1 scan.
After that it periodically cycles between reception and transmission.
Initially it is in un-synchronized mode which means it uses the fixed, pre-configured period for reception and to transmit its messages.

.. code-block::

  period(unsynchronized) = CONFIG_DECTNRP_SAMPLE_PERIOD

  |<---------------- period ---------------->|<---------------- period ---------------->|
  | RX                                    |TX| RX                                    |TX|   ...  


**Synchronization process**

Above scheme will demonstrate imediate scheduling of rx and tx operations (start_time=0). Only one device is needed.
To also demonstrate modem-based time scheduling (start_time is a valid modem-time in the future) you need a second device.

For this sample a simple ping-pong communication is choosen, where both devices do not have to know each other in advance.
They synchronize against another using the start-time of received messages.

When Device 2 received a message transmitted from Device 1 it also has got the start-time of that message. 
Based on that start-time it alligns its own transmission and reception to the cycle of the Device 1.
Together with that messge it also learns the short-id of the remote device 

.. code-block::

  Period(unsynchronized) = CONFIG_DECTNRP_SAMPLE_PERIOD
  Period(synchronized) = CONFIG_DECTNRP_SAMPLE_PERIOD/2

.. image:: synchronization.svg

* Device 1 does TX-RX with pre-configured unsynchronized period (starting with RX)
* Device 2 does TX-RX with pre-configured unsynchronized period (also starting with RX)
* Device 2 receives message M1 sent by Device 1 (RX in single shot mode)

  * It got the start time of M1 from its driver and synchronizes sending of its own message M2 to synchronized period/2
  * It also learned the short id of the remote device and is in SYNCHRONIZED mode now
* Device 1 receives message M2 sent by Device 2 (RX in single shot mode)

  * It got the start time of M2 from its driver and also synchronizes sending of its own message M1 to synchronized period/2
  * It also learned the short id of the remote device and is in SYNCHRONIZED mode now

When no message has been received within the rx operation window the state changes back into UNSYNCHRONIZED mode.

Usage
*****

This sample is currently meant to run on nRF9151dk board(s). 
For single device tests you can run it on one board. For testing real communication you should have two of them ready.

If you are using vscode the attached `sample-linux.code-workspace <../../sample-linux.code-workspace>`_ contains some build and flash tasks and additionally debug launch configurations.

* Adjust ``CONFIG_DECTNRP_SAMPLE_CHANNEL`` and other project settings in `prj.conf <prj.conf>`_ to your needs.
* Assuming you have one/two SEGGER JLink override there serials (see ``device1-serial-number`` and ``device2-serial-number``) to ``.vscode/settings.json`` or directly override values in `sample-linux.code-workspace <../../sample-linux.code-workspace>`_.
* Build sample *dectnrp_driver* as descriped in top-level README.md or using vscode tasks.
* Connect one nrf9151dk (device 1) via USB.
* When you directly run ``Device1(launch)``

  * the sample should be uploaded
  * a RTT-terminal ``Device 1`` with the log should be opened
  * the sample should stop at main()
* If you continue the debugger the logger ``Device 1`` should show something like this:

.. code-block::

    [00:00:00.549,407] <wrn> dectnrp_nrf91x1: MFW version:mfw-nr+-phy_nrf91x1_2.0.0
    [00:00:00.549,865] <wrn> dectnrp_nrf91x1: MFW uuid:75a511eb-5f17-488b-bb61-b1d29bfaabe0
    [00:00:00.549,896] <wrn> dectnrp_nrf91x1: MFW lib-version:3.2.2-dectphy-369d28b2e47c
    [00:00:00.694,030] <inf> dectnrp_nrf91x1: nrf91x1 dectnrp radio initialized
    [00:00:00.694,244] <inf> dectnrp_nrf91x1: nrf91x1_configure(dectnrp_nrf91x1)
    [00:00:00.694,244] <inf> dectnrp_nrf91x1: Iface initialized
    [00:00:00.695,037] <inf> dectnrp_nrf91x1: nrf91x1_start(dectnrp_nrf91x1)
    [00:00:00.742,797] <inf> dectnrp_nrf91x1: DECT radio nrf91 started
    *** Booting nRF Connect SDK v3.2.4-4c3fc0d44534 ***
    *** Using Zephyr OS v4.2.99-9673eec75908 ***
    [00:01:02.265,686] <inf> dectnrp_nrf91x1: nrf91x1_configure(dectnrp_nrf91x1)
    [00:01:02.266,448] <inf> dectnrp_driver_sample: -------------------------
    [00:01:02.266,479] <inf> dectnrp_driver_sample: Local device:
    [00:01:02.266,479] <inf> dectnrp_driver_sample:  DEVICE_STATE_UNSYNCHRONIZED
    [00:01:02.266,479] <inf> dectnrp_driver_sample:  - network id 0x32545279
    [00:01:02.266,510] <inf> dectnrp_driver_sample:  - long device id 0xa7a95ba7
    [00:01:02.266,510] <inf> dectnrp_driver_sample:  - short device id 0xf3c0
    [00:01:02.266,540] <inf> dectnrp_driver_sample:  - channel 1677
    [00:01:02.266,540] <inf> dectnrp_driver_sample: -------------------------
    [00:01:02.266,693] <inf> dectnrp_nrf91x1: <4266259861> nrf91x1_get_time()
    [00:01:02.266,754] <inf> dectnrp_driver_sample: Network time=4266259861
    [00:01:02.278,167] <inf> dectnrp_driver_sample: RSSI1 scan on carrier:1677
    [00:01:02.278,228] <inf> dectnrp_driver_sample: subslots[dBm]:[-81,-82,-107,-106,-106,-106,-106,-106,-106,-106,-106,-106,-106,-106,-106,-107,-106,-106,-106,-106,-106,-106,-106,-106,-106,-106,-106,-107,-106,-107,-106,-106,-106,-106,-107,-106,-106,-106,-106,-106,-106,-106,-107,-106,-106,-106,-82,-83 ]
    [00:01:02.278,289] <inf> dectnrp_driver_sample: receive ...
    [00:01:05.279,022] <inf> dectnrp_driver_sample: receive complete
    [00:01:05.279,327] <inf> dectnrp_driver_sample: transmit
    [00:01:05.280,273] <inf> dectnrp_driver_sample: transmit complete

* The sample just listens but does not receive anything but sends its own message periodically.

* Now connect second nrf9151dk (device 2) via USB.
* When you directly run ``Device2(launch)``

  * the sample should also be uploaded
  * a RTT-terminal ``Device 2`` with the log should be opened
  * the sample should stop at main()
* If you continue the debugger the logger of should show something like this:

.. code-block::

    [00:00:00.581,268] <wrn> dectnrp_nrf91x1: MFW version:mfw-nr+-phy_nrf91x1_2.0.0
    [00:00:00.581,726] <wrn> dectnrp_nrf91x1: MFW uuid:75a511eb-5f17-488b-bb61-b1d29bfaabe0
    [00:00:00.581,756] <wrn> dectnrp_nrf91x1: MFW lib-version:3.2.2-dectphy-369d28b2e47c
    [00:00:00.725,860] <inf> dectnrp_nrf91x1: nrf91 dectnrp radio initialized
    [00:00:00.726,074] <inf> dectnrp_nrf91x1: nrf91x1_configure(dectnrp_nrf91x1)
    [00:00:00.726,074] <inf> dectnrp_nrf91x1: Iface initialized
    [00:00:00.726,867] <inf> dectnrp_nrf91x1: nrf91x1_start(dectnrp_nrf91x1)
    [00:00:00.774,627] <inf> dectnrp_nrf91x1: DECT radio nrf91 started
    *** Booting nRF Connect SDK v3.2.4-4c3fc0d44534 ***
    *** Using Zephyr OS v4.2.99-9673eec75908 ***
    [00:01:02.804,595] <inf> dectnrp_nrf91x1: nrf91x1_configure(dectnrp_nrf91x1)
    [00:01:02.805,389] <inf> dectnrp_driver_sample: -------------------------
    [00:01:02.805,389] <inf> dectnrp_driver_sample: Local device:
    [00:01:02.805,419] <inf> dectnrp_driver_sample:  DEVICE_STATE_UNSYNCHRONIZED
    [00:01:02.805,419] <inf> dectnrp_driver_sample:  - network id 0x32545279
    [00:01:02.805,450] <inf> dectnrp_driver_sample:  - long device id 0x2959b498
    [00:01:02.805,450] <inf> dectnrp_driver_sample:  - short device id 0xc061
    [00:01:02.805,450] <inf> dectnrp_driver_sample:  - channel 1677
    [00:01:02.805,480] <inf> dectnrp_driver_sample: -------------------------
    [00:01:02.805,633] <inf> dectnrp_nrf91x1: <4301387499> nrf91x1_get_time()
    [00:01:02.805,694] <inf> dectnrp_driver_sample: Network time=4301387499
    [00:01:02.817,077] <inf> dectnrp_driver_sample: RSSI1 scan on carrier:1677
    [00:01:02.817,169] <inf> dectnrp_driver_sample: subslots[dBm]:[-105,-106,-105,-105,-106,-106,-105,-105,-105,-105,-106,-105,-105,-105,-105,-105,-105,-105,-105,-104,-105,-105,-106,-105,-105,-105,-105,-105,-105,-105,-104,-105,-105,-83,-84,-83,-82,-105,-105,-106,-105,-105,-105,-105,-104,-104,-105,-105 ]
    [00:01:02.817,199] <inf> dectnrp_driver_sample: receive ...
    [00:01:05.152,465] <inf> dectnrp_driver_sample: RX:PCC:
                                                    10 79 f3 c0 71 ff ff 00  00 00                   |.y..q... ..      
    [00:01:05.152,526] <inf> dectnrp_driver_sample: PDC:
                                                    00 00 17 43 0d 48 65 6c  6c 6f 20 57 6f 72 6c 64 |...C.Hel lo World
                                                    21 0a 40 11 00 00 00 00  00 00 00 00 00 00 00 00 |!.@..... ........
                                                    00 00 00 00 00                                   |.....            
    [00:01:05.152,557] <inf> dectnrp_driver_sample: receive complete
    [00:01:06.652,709] <inf> dectnrp_driver_sample: -------------------------
    [00:01:06.652,740] <inf> dectnrp_driver_sample: Local device:
    [00:01:06.652,740] <inf> dectnrp_driver_sample:  DEVICE_STATE_SYNCHRONIZED
    [00:01:06.652,740] <inf> dectnrp_driver_sample: Remote device:
    [00:01:06.652,770] <inf> dectnrp_driver_sample:  - short device id 0xf3c0
    [00:01:06.652,770] <inf> dectnrp_driver_sample: -------------------------
    [00:01:06.653,045] <inf> dectnrp_driver_sample: transmit
    [00:01:06.654,022] <inf> dectnrp_driver_sample: transmit complete
    [00:01:06.654,144] <inf> dectnrp_driver_sample: receive ...
    [00:01:08.155,975] <inf> dectnrp_driver_sample: RX:PCC:
                                                    10 79 f3 c0 71 c0 61 00  00 00                   |.y..q.a. ..      
    [00:01:08.156,036] <inf> dectnrp_driver_sample: PDC:
                                                    00 00 17 43 0d 48 65 6c  6c 6f 20 57 6f 72 6c 64 |...C.Hel lo World
                                                    21 0a 40 11 00 00 00 00  00 00 00 00 00 00 00 00 |!.@..... ........
                                                    00 00 00 00 00                                   |.....            
    [00:01:08.156,066] <inf> dectnrp_driver_sample: receive complete
    [00:01:09.656,463] <inf> dectnrp_driver_sample: transmit
    [00:01:09.657,440] <inf> dectnrp_driver_sample: transmit complete
    [00:01:09.657,562] <inf> dectnrp_driver_sample: receive ...
    [00:01:11.159,332] <inf> dectnrp_driver_sample: RX:PCC:
                                                    10 79 f3 c0 71 c0 61 00  00 00                   |.y..q.a. ..      
    [00:01:11.159,362] <inf> dectnrp_driver_sample: PDC:
                                                    00 00 17 43 0d 48 65 6c  6c 6f 20 57 6f 72 6c 64 |...C.Hel lo World
                                                    21 0a 40 11 00 00 00 00  00 00 00 00 00 00 00 00 |!.@..... ........
                                                    00 00 00 00 00                                   |.....            
    [00:01:11.159,423] <inf> dectnrp_driver_sample: receive complete
    [00:01:12.659,790] <inf> dectnrp_driver_sample: transmit
    [00:01:12.660,766] <inf> dectnrp_driver_sample: transmit complete


* Also the logger ``Device 1`` should now have logged out progress:

.. code-block::

    [00:01:14.285,339] <inf> dectnrp_driver_sample: receive complete
    [00:01:14.285,614] <inf> dectnrp_driver_sample: transmit
    [00:01:14.286,590] <inf> dectnrp_driver_sample: transmit complete
    [00:01:14.286,712] <inf> dectnrp_driver_sample: receive ...
    [00:01:15.788,604] <inf> dectnrp_driver_sample: RX:PCC:
                                                    10 79 c0 61 71 f3 c0 00  00 00                   |.y.aq... ..      
    [00:01:15.788,665] <inf> dectnrp_driver_sample: PDC:
                                                    00 00 17 43 0d 48 65 6c  6c 6f 20 57 6f 72 6c 64 |...C.Hel lo World
                                                    21 0a 40 11 00 00 00 00  00 00 00 00 00 00 00 00 |!.@..... ........
                                                    00 00 00 00 00                                   |.....            
    [00:01:15.788,696] <inf> dectnrp_driver_sample: receive complete
    [00:01:17.288,848] <inf> dectnrp_driver_sample: -------------------------
    [00:01:17.288,879] <inf> dectnrp_driver_sample: Local device:
    [00:01:17.288,879] <inf> dectnrp_driver_sample:  DEVICE_STATE_SYNCHRONIZED
    [00:01:17.288,879] <inf> dectnrp_driver_sample: Remote device:
    [00:01:17.288,909] <inf> dectnrp_driver_sample:  - short device id 0xc061
    [00:01:17.288,909] <inf> dectnrp_driver_sample: -------------------------
    [00:01:17.289,184] <inf> dectnrp_driver_sample: transmit
    [00:01:17.290,161] <inf> dectnrp_driver_sample: transmit complete
    [00:01:17.290,283] <inf> dectnrp_driver_sample: receive ...
    [00:01:18.792,083] <inf> dectnrp_driver_sample: RX:PCC:
                                                    10 79 c0 61 71 f3 c0 00  00 00                   |.y.aq... ..      
    [00:01:18.792,144] <inf> dectnrp_driver_sample: PDC:
                                                    00 00 17 43 0d 48 65 6c  6c 6f 20 57 6f 72 6c 64 |...C.Hel lo World
                                                    21 0a 40 11 00 00 00 00  00 00 00 00 00 00 00 00 |!.@..... ........
                                                    00 00 00 00 00                                   |.....            
    [00:01:18.792,175] <inf> dectnrp_driver_sample: receive complete
    [00:01:20.292,572] <inf> dectnrp_driver_sample: transmit
    [00:01:20.293,548] <inf> dectnrp_driver_sample: transmit complete

* You can see that both devices synchronize to one another and exchange there messages with a period of ``CONFIG_DECTNRP_SAMPLE_PERIOD/2``.
