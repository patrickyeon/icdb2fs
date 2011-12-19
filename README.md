icdb2fs
=======

Mentor Graphics' DxDesigner suite stores schematic data in a binary file called icdb.dat, with no format spec (that I could find). In the interest of interoperability and fighting vendor lock-in, icdb2fs is meant to be a first step in being able to access the schematic data. Upon investigation, it appears that icdb.dat is a representation of a file system; icdb2fs will extract that file system.

Usage
-----

`icdb2fs t filename` to get a listing of all included files.
`icdb2fs x filename [path]` to extract them, default target is the current directory.

Warning!
--------

This was done using only a very few example files to work with, so it's quite possible there are valid files that will cause it to completely choke, simply because assumptions turn out wrong. There is also no defense against malicious files (off-hand, there is no directory traversal protection, and there is at least one point where a malicious file could send us in an infinite loop). Please don't blindly trust it.

Legal Notes
-----------

All the research and programming included was done by Patrick Yeon, working solely from icdb.dat files output by DxDesigner and written to the filesystem. There was no static nor dynamic analysis of executable files, nor any examination of run time memory contents. All the work was undertaken in Canada. It is my belief that this makes the work perfectly legal, as reverse engineering with the intent of enabling interoperation is one of the most widely respected forms of RE.

Until I have had an opportunity to investigate legal matters going forward, patches will be ignored.

License
-------

This software is licensed under the 2-clause BSD license ("Simplified BSD License"), Copyright 2011 Patrick Yeon. Of course, I'd love to hear if anyone uses it anywhere.
