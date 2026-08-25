# blip_buf used by Ear6

This directory contains the vendored blip_buf 1.1.0 source used by the NES
audio mixer. Ear6 extends the original interface with state accessors so a save
state can restore the mixer without losing future audio determinism.

Keep those local extensions visible when updating the upstream source.
