# midi / midi_isa : MIDI plugin for mxPlay and Jam

For any MIDI enabled Atari or clone.

midi.mxp / midi.jam plays through Atari Bios routines and should
work on any Atari as long as it implements midi bios.

midi_isa.mxp / midi_isa.jam plays through an MPU401 compatible ISA device
Device is assumed to be located on 0x330 unless otherwise detected in ISA_BIOS

ISA_BIOS is recommended but it can work without on some recognised computer types
https://github.com/agranlund/raven/tree/main/sw/isa/isa_bios

---

MXP plugin is for use with mxPlay (c) Miro Kropacek.
 https://mxplay.sourceforge.net

JAM plugin is for use with Jam (c) Cream.
 https://www.creamhq.de/jam

Plugin is based on MD_MIDIFile (c) Marco Colli.
 https://github.com/MajicDesigns/MD_MIDIFile

---

## MD_MIDIFile

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

