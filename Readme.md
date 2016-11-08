 
=============================
Wiener Deconvolution
=============================

This Application enables you to apply deblur/enhance images using Wiener Deconvolution on Maxeler DFEs


Introduction
------------

This APP showcases how Image enhancement using Wiener Deconvolution can be implemented using MaxJ and using the MaxPower library provided by Maxeler.


Features
--------

* Overlap Add structure allows for large images to be processed. Up to 48 GB are possible depending on the filter size
* Filter kernel import from Matlab is supported (any other tool which can output CSV files works)
* 1.8 GPixel/s throughput at 64x64 kernel size. Smaller kernel sizes lead to a faster operation
