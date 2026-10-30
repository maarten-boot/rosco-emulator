-- license:BSD-3-Clause
-- copyright-holders:MAMEdev Team

---------------------------------------------------------------------------
--
--   rosco.lua
--
--   Build target for the rosco_m68k family of single board computers.
--
--   The enabled component set below is what the rosco drivers pull in
--   (see scripts/build/makedep.py), plus the CPU cores that are kept
--   available on purpose for the boards that are not emulated yet.
--
---------------------------------------------------------------------------

--------------------------------------------------
-- CPU cores
--------------------------------------------------

-- 68000/68010/68020/68030 - rosco_m68k Classic, Classic v2 and r1
CPUS["M680X0"] = true

-- W65C02S - rosco_6502
CPUS["M6502"] = true

--------------------------------------------------
-- Machines used by the rosco boards
--------------------------------------------------

MACHINES["68681"] = true        -- XR68C681 DUART
MACHINES["SPISDCARD"] = true    -- bit-banged SPI SD card
MACHINES["INTELFLASH"] = true   -- SST39SF040 flash ROM chips

--------------------------------------------------
-- IDE/ATA interface
--------------------------------------------------

BUSES["ATA"] = true
MACHINES["ATASTORAGE"] = true
MACHINES["ATAHLE"] = true
SOUNDS["CDDA"] = true
CPUS["FR"] = true
CPUS["MCS51"] = true
CPUS["MC68HC11"] = true

--------------------------------------------------
-- Serial ports and the terminals that can be
-- plugged into them
--------------------------------------------------

BUSES["RS232"] = true
BUSES["SUNKBD"] = true
BUSES["HEATHZENITH_H19"] = true
CPUS["IE15"] = true
CPUS["M6800"] = true
CPUS["MCS48"] = true
CPUS["Z80"] = true
MACHINES["6821PIA"] = true
MACHINES["ACIA6850"] = true
MACHINES["IE15"] = true
MACHINES["INPUT_MERGER"] = true
MACHINES["INS8250"] = true
MACHINES["MM5740"] = true
MACHINES["PCF8573"] = true
MACHINES["S97801"] = true
MACHINES["SCN_PCI"] = true
MACHINES["SWTPC8212"] = true
MACHINES["VOTRAXTNT"] = true
MACHINES["Z80DAISY"] = true
SOUNDS["AY8910"] = true
SOUNDS["BEEP"] = true
SOUNDS["VOTRAX_SC01"] = true
SOUNDS["VOTRAX_SC01A"] = true
VIDEOS["MC6845"] = true
VIDEOS["SCN2674"] = true

--------------------------------------------------
-- Drivers
--------------------------------------------------

function createProjects_rosco_rosco(_target, _subtarget)
	project ("rosco_rosco")
	targetsubdir(_target .."_" .. _subtarget)
	kind (LIBTYPE)
	uuid (os.uuid("drv-rosco-rosco"))
	addprojectflags()
	precompiledheaders_novs()

	includedirs {
		MAME_DIR .. "src/osd",
		MAME_DIR .. "src/emu",
		MAME_DIR .. "src/devices",
		MAME_DIR .. "src/rosco",
		MAME_DIR .. "src/lib",
		MAME_DIR .. "src/lib/util",
		MAME_DIR .. "3rdparty",
		GEN_DIR  .. "rosco/layout",
	}

	files {
		MAME_DIR .. "src/rosco/drivers/rosco_m68k.cpp",
		MAME_DIR .. "src/rosco/drivers/rosco_6502.cpp",
	}
end

function linkProjects_rosco_rosco(_target, _subtarget)
	links {
		"rosco_rosco",
	}
end
