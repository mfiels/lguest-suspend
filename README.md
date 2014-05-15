lguest-suspend
==============

Lguest with suspend, resume, snapshotting added.

Ideas to fix fault in Switcher
==============================
1. Instead of storing all of the lguest data fields in the state group, store the pointer. Look more into where lguest data is initialized.
2. Remove check on guest page table flush when hypercalling for the first time on restore
