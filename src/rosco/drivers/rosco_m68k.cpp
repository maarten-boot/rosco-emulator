// license:BSD-3-Clause
// copyright-holders:Ross Bamford, Chris Hanson
/*
 *------------------------------------------------------------
 *                                  ___ ___ _
 *  ___ ___ ___ ___ ___       _____|  _| . | |_
 * |  _| . |_ -|  _| . |     |     | . | . | '_|
 * |_| |___|___|___|___|_____|_|_|_|___|___|_,_|
 *                     |_____|
 * ------------------------------------------------------------
 * Copyright (c) 2024 The rosco_m68k Open Source Project
 * MIT License
 *
 * Portions (c) Chris Hanson
 * BSD 3-clause License
 * ------------------------------------------------------------
 *
 * rosco_m68k Classic and Classic v2 single board computers.
 *
 * Hardware:
 * - MC68000/68010 @ 10MHz, or MC68020/68030 @ 20MHz on the CPU upgrade
 * - 1MB DRAM at 0x000000, 1MB (max) ROM at 0xe00000
 * - optional RAM expansion directly above it, contiguous up to 14MB in
 *   total (-ram 5M for the 4MB expansion board); the firmware sizes it
 * - XR68C681 DUART at 0xf00000 (IRQ 4), two RS-232 channels
 * - SPI SD card bit-banged off the DUART output port
 * - IDE/ATA interface at 0xf80040 (IRQ 3)
 *
 * The first eight bytes of the address space are overlaid with ROM at
 * reset so that the CPU fetches its initial SSP/PC from the monitor; the
 * overlay is dropped as soon as anything writes to that area.
 *
 * Unmapped areas of the address space assert BERR, which the monitor
 * relies on when it sizes memory and probes for expansion hardware.
 */

#include "emu.h"

#include "bus/ata/ataintf.h"
#include "bus/rs232/rs232.h"
#include "cpu/m68000/m68000.h"
#include "cpu/m68000/m68010.h"
#include "cpu/m68000/m68020.h"
#include "cpu/m68000/m68030.h"
#include "imagedev/snapquik.h"
#include "machine/mc68681.h"
#include "machine/ram.h"
#include "machine/spi_sdcard.h"


namespace {

class rosco_m68k_state : public driver_device
{
public:
	rosco_m68k_state(const machine_config &mconfig, device_type type, const char *tag)
		: driver_device(mconfig, type, tag)
		, m_maincpu(*this, "maincpu")
		, m_duart(*this, "duart")
		, m_terminal(*this, "terminal")
		, m_host(*this, "host")
		, m_sdcard(*this, "sdcard")
		, m_ata(*this, "ata")
		, m_ram(*this, RAM_TAG)
	{ }

protected:
	required_device<m68000_base_device> m_maincpu;
	required_device<xr68c681_device> m_duart;
	bool m_bus_error = false;

	void rosco_m68k(machine_config &config);

	DECLARE_QUICKLOAD_LOAD_MEMBER(quickload_cb);

	void mem_map(address_map &map) ATTR_COLD;
	void cpu_space_map(address_map &map) ATTR_COLD;

	virtual void delegated_mem_map(address_map &map) ATTR_COLD = 0;
	virtual void delegated_cpu_space_map(address_map &map) ATTR_COLD = 0;
	virtual void bootvec_reset() = 0;

	// The 68000 core and the Musashi cores signal bus errors differently.
	virtual void signal_bus_error(uint32_t address, bool read) = 0;

	// Base is the start of the range the handler is mapped at: handlers are
	// passed offsets relative to it, not the address the CPU put out.
	template <offs_t Base> uint16_t unmapped_r(offs_t offset, uint16_t mem_mask);
	template <offs_t Base> void unmapped_w(offs_t offset, uint16_t data, uint16_t mem_mask);
	void set_bus_error(uint32_t address, bool read);

private:
	required_device<rs232_port_device> m_terminal;
	required_device<rs232_port_device> m_host;

	required_device<spi_sdcard_device> m_sdcard;
	required_device<ata_interface_device> m_ata;
	required_device<ram_device> m_ram;

	emu_timer *m_bus_error_timer = nullptr;

	void write_red_led(int state);
	void write_green_led(int state);

	virtual void machine_start() override ATTR_COLD;
	virtual void machine_reset() override ATTR_COLD;

	TIMER_CALLBACK_MEMBER(bus_error);
};


// Shared behaviour for the Musashi-based CPUs (68010/68020/68030).
class rosco_m68k_musashi_state : public rosco_m68k_state
{
protected:
	using rosco_m68k_state::rosco_m68k_state;

	virtual void signal_bus_error(uint32_t address, bool read) override;
};


class rosco_m68k_000_state : public rosco_m68k_state
{
public:
	rosco_m68k_000_state(const machine_config &mconfig, device_type type, const char *tag)
		: rosco_m68k_state(mconfig, type, tag)
		, m_bootvect(*this, "bootvect")
		, m_sysram(*this, "sysram")
	{ }

	void rosco_m68k_000(machine_config &config);

private:
	memory_view m_bootvect;
	required_shared_ptr<uint16_t> m_sysram; // Pointer to System RAM needed by bootvect_w and masking RAM buffer for post reset accesses

	virtual void delegated_mem_map(address_map &map) override ATTR_COLD;
	virtual void delegated_cpu_space_map(address_map &map) override ATTR_COLD;

	void bootvect_w(offs_t offset, uint16_t data, uint16_t mem_mask = ~0);

	virtual void bootvec_reset() override;
	virtual void signal_bus_error(uint32_t address, bool read) override;
};

class rosco_m68k_010_state : public rosco_m68k_musashi_state
{
public:
	rosco_m68k_010_state(const machine_config &mconfig, device_type type, const char *tag)
		: rosco_m68k_musashi_state(mconfig, type, tag)
		, m_bootvect(*this, "bootvect")
		, m_sysram(*this, "sysram")
	{ }

	void rosco_m68k_010(machine_config &config);

private:
	memory_view m_bootvect;
	required_shared_ptr<uint16_t> m_sysram;

	virtual void delegated_mem_map(address_map &map) override ATTR_COLD;
	virtual void delegated_cpu_space_map(address_map &map) override ATTR_COLD;

	void bootvect_w(offs_t offset, uint16_t data, uint16_t mem_mask = ~0);

	virtual void bootvec_reset() override;
};

class rosco_m68k_020_state : public rosco_m68k_musashi_state
{
public:
	rosco_m68k_020_state(const machine_config &mconfig, device_type type, const char *tag)
		: rosco_m68k_musashi_state(mconfig, type, tag)
		, m_bootvect(*this, "bootvect")
		, m_sysram(*this, "sysram")
	{ }

	void rosco_m68k_020(machine_config &config);

private:
	memory_view m_bootvect;
	required_shared_ptr<uint32_t> m_sysram;

	virtual void delegated_mem_map(address_map &map) override ATTR_COLD;
	virtual void delegated_cpu_space_map(address_map &map) override ATTR_COLD;

	void bootvect_w(offs_t offset, uint32_t data, uint32_t mem_mask = ~0);

	virtual void bootvec_reset() override;
};

class rosco_m68k_030_state : public rosco_m68k_musashi_state
{
public:
	rosco_m68k_030_state(const machine_config &mconfig, device_type type, const char *tag)
		: rosco_m68k_musashi_state(mconfig, type, tag)
		, m_bootvect(*this, "bootvect")
		, m_sysram(*this, "sysram")
	{ }

	void rosco_m68k_030(machine_config &config);

private:
	memory_view m_bootvect;
	required_shared_ptr<uint32_t> m_sysram;

	virtual void delegated_mem_map(address_map &map) override ATTR_COLD;
	virtual void delegated_cpu_space_map(address_map &map) override ATTR_COLD;

	void bootvect_w(offs_t offset, uint32_t data, uint32_t mem_mask = ~0);

	virtual void bootvec_reset() override;
};


/* Input ports */
static INPUT_PORTS_START( rosco_m68k )
INPUT_PORTS_END


/* Terminal default settings. */
static DEVICE_INPUT_DEFAULTS_START(terminal)
	DEVICE_INPUT_DEFAULTS( "RS232_RXBAUD", 0xff, RS232_BAUD_115200 )
	DEVICE_INPUT_DEFAULTS( "RS232_TXBAUD", 0xff, RS232_BAUD_115200 )
	DEVICE_INPUT_DEFAULTS( "RS232_DATABITS", 0xff, RS232_DATABITS_8 )
	DEVICE_INPUT_DEFAULTS( "RS232_PARITY", 0xff, RS232_PARITY_NONE )
	DEVICE_INPUT_DEFAULTS( "RS232_STOPBITS", 0xff, RS232_STOPBITS_1 )
DEVICE_INPUT_DEFAULTS_END


void rosco_m68k_000_state::rosco_m68k_000(machine_config &config)
{
	M68000(config, m_maincpu, 10_MHz_XTAL);

	rosco_m68k(config);
}

void rosco_m68k_010_state::rosco_m68k_010(machine_config &config)
{
	M68010(config, m_maincpu, 10_MHz_XTAL);

	rosco_m68k(config);
}

void rosco_m68k_020_state::rosco_m68k_020(machine_config &config)
{
	M68020(config, m_maincpu, 20_MHz_XTAL);

	rosco_m68k(config);
}

void rosco_m68k_030_state::rosco_m68k_030(machine_config &config)
{
	M68030(config, m_maincpu, 20_MHz_XTAL);

	rosco_m68k(config);
}

void rosco_m68k_state::rosco_m68k(machine_config &config)
{
	m_maincpu->set_addrmap(AS_PROGRAM, &rosco_m68k_state::mem_map);
	m_maincpu->set_addrmap(m68000_base_device::AS_CPU_SPACE, &rosco_m68k_state::cpu_space_map);

	// Set up DUART, both binding to serial ports and handling GPIO.
	// IP0 = CTS_A
	// IP1 = CTS_B
	// IP2 = SPI_CIPO
	// IP3 = ???
	// IP4 = ???
	// IP5 = ???
	//
	// OP0 = RTS_A
	// OP1 = RTS_B
	// OP2 = SPI_CS
	// OP3 = RED_LED
	// OP4 = SPI_SCK
	// OP5 = GREEN_LED
	// OP6 = SPI_COPI
	// OP7 = SPI_CS1

	// The DUART runs from its own 3.6864MHz crystal, not the CPU clock - the
	// baud rates and the firmware's 100Hz timer tick both depend on it.
	XR68C681(config, m_duart, 3.6864_MHz_XTAL);
	m_duart->irq_cb().set_inputline(m_maincpu, M68K_IRQ_4);
	m_duart->a_tx_cb().set(m_terminal, FUNC(rs232_port_device::write_txd));
	m_duart->outport_cb().set(m_terminal, FUNC(rs232_port_device::write_rts)).bit(0);
	m_duart->b_tx_cb().set(m_host, FUNC(rs232_port_device::write_txd));
	m_duart->outport_cb().append(m_host, FUNC(rs232_port_device::write_rts)).bit(1);
	m_duart->outport_cb().append(FUNC(rosco_m68k_state::write_red_led)).bit(3);
	m_duart->outport_cb().append(FUNC(rosco_m68k_state::write_green_led)).bit(5);
	m_duart->outport_cb().append(m_sdcard, FUNC(spi_sdcard_device::spi_ss_w)).bit(2).invert();
	m_duart->outport_cb().append(m_sdcard, FUNC(spi_sdcard_device::spi_clock_w)).bit(4);
	m_duart->outport_cb().append(m_sdcard, FUNC(spi_sdcard_device::spi_mosi_w)).bit(6);

	RS232_PORT(config, m_terminal, default_rs232_devices, "terminal");
	m_terminal->rxd_handler().set(m_duart, FUNC(xr68c681_device::rx_a_w));
	m_terminal->set_option_device_input_defaults("terminal", DEVICE_INPUT_DEFAULTS_NAME(terminal));
	m_terminal->set_option_device_input_defaults("null_modem", DEVICE_INPUT_DEFAULTS_NAME(terminal));
	m_terminal->set_option_device_input_defaults("pty", DEVICE_INPUT_DEFAULTS_NAME(terminal));
	m_terminal->cts_handler().set(m_duart, FUNC(xr68c681_device::ip0_w));

	RS232_PORT(config, m_host, default_rs232_devices, nullptr);
	m_host->rxd_handler().set(m_duart, FUNC(xr68c681_device::rx_b_w));
	m_host->set_option_device_input_defaults("terminal", DEVICE_INPUT_DEFAULTS_NAME(terminal));
	m_host->set_option_device_input_defaults("null_modem", DEVICE_INPUT_DEFAULTS_NAME(terminal));
	m_host->set_option_device_input_defaults("pty", DEVICE_INPUT_DEFAULTS_NAME(terminal));
	m_host->cts_handler().set(m_duart, FUNC(xr68c681_device::ip1_w));

	SPI_SDCARD(config, m_sdcard, 0);
	m_sdcard->spi_miso_callback().set(m_duart, FUNC(xr68c681_device::ip2_w));

	// Total RAM: the 1MB on board plus the expansion above it, which
	// machine_start() maps in place of the bus error area.
	RAM(config, m_ram).set_default_size("1M").set_extra_options("2M,3M,4M,5M,6M,8M,10M,12M,14M");

	ATA_INTERFACE(config, m_ata, 0).options(ata_devices, "hdd", nullptr, false);
	m_ata->irq_handler().set_inputline(m_maincpu, M68K_IRQ_3);

	// Drop a raw binary straight into memory instead of sending it over
	// Kermit or putting it on the SD card. The delay lets the firmware
	// finish bringing the hardware up before control is handed over.
	QUICKLOAD(config, "quickload", "bin,rom", attotime::from_seconds(3))
			.set_load_callback(FUNC(rosco_m68k_state::quickload_cb));
}

/*
 * Programs are linked to load and start at 0x40000, which is where the
 * firmware puts them when they arrive over Kermit.
 */
QUICKLOAD_LOAD_MEMBER(rosco_m68k_state::quickload_cb)
{
	constexpr offs_t LOAD_ADDRESS = 0x40000;
	const offs_t ram_end = m_ram->size();

	const uint64_t length = image.length();

	if (!length || (length > (ram_end - LOAD_ADDRESS)))
		return std::make_pair(image_error::INVALIDLENGTH, std::string());

	std::vector<uint8_t> buffer(length);
	if (image.fread(&buffer[0], length) != length)
		return std::make_pair(image_error::UNSPECIFIED, "Error reading file");

	address_space &space = m_maincpu->space(AS_PROGRAM);
	for (offs_t i = 0; i < length; i++)
		space.write_byte(LOAD_ADDRESS + i, buffer[i]);

	m_maincpu->set_pc(LOAD_ADDRESS);

	return std::make_pair(std::error_condition(), std::string());
}

void rosco_m68k_state::write_red_led(int state)
{
	// Nothing yet
}

void rosco_m68k_state::write_green_led(int state)
{
	// Nothing yet
}

void rosco_m68k_state::mem_map(address_map &map)
{
	map(0x000000, 0x0fffff).ram().share("sysram"); /* 1MB RAM */
	map(0xe00000, 0xefffff).rom().region("monitor", 0); /* 1MB ROM (max) */
	map(0xf00000, 0xf0001f).rw(m_duart, FUNC(xr68c681_device::read), FUNC(xr68c681_device::write)).umask16(0x00ff);
	map(0xf80040, 0xf8004f).rw(m_ata, FUNC(ata_interface_device::cs0_r), FUNC(ata_interface_device::cs0_w)).umask16(0xffff);
	map(0xf80050, 0xf8005f).rw(m_ata, FUNC(ata_interface_device::cs1_r), FUNC(ata_interface_device::cs1_w)).umask16(0xffff);

	// Unmapped areas to bus error...
	map(0x100000, 0xdfffff).rw(FUNC(rosco_m68k_state::unmapped_r<0x100000>), FUNC(rosco_m68k_state::unmapped_w<0x100000>));
	map(0xf00020, 0xf8003f).rw(FUNC(rosco_m68k_state::unmapped_r<0xf00020>), FUNC(rosco_m68k_state::unmapped_w<0xf00020>));
	map(0xf80060, 0xfffdff).rw(FUNC(rosco_m68k_state::unmapped_r<0xf80060>), FUNC(rosco_m68k_state::unmapped_w<0xf80060>));

	delegated_mem_map(map);
}

void rosco_m68k_000_state::delegated_mem_map(address_map &map)
{
	map(0x000000, 0x000007).view(m_bootvect);
	m_bootvect[0](0x000000, 0x000007).rom().region("monitor", 0);                // After first write we act as RAM
	m_bootvect[0](0x000000, 0x000007).w(FUNC(rosco_m68k_000_state::bootvect_w)); // ROM mirror just during reset
}

void rosco_m68k_010_state::delegated_mem_map(address_map &map)
{
	map(0x000000, 0x000007).view(m_bootvect);
	m_bootvect[0](0x000000, 0x000007).rom().region("monitor", 0);                // After first write we act as RAM
	m_bootvect[0](0x000000, 0x000007).w(FUNC(rosco_m68k_010_state::bootvect_w)); // ROM mirror just during reset
}

void rosco_m68k_020_state::delegated_mem_map(address_map &map)
{
	map(0x000000, 0x000007).view(m_bootvect);
	m_bootvect[0](0x000000, 0x000007).rom().region("monitor", 0);                // After first write we act as RAM
	m_bootvect[0](0x000000, 0x000007).w(FUNC(rosco_m68k_020_state::bootvect_w)); // ROM mirror just during reset

	// Additional address space to bus error
	map(0x01000000, 0xffffffff).rw(FUNC(rosco_m68k_020_state::unmapped_r<0x01000000>), FUNC(rosco_m68k_020_state::unmapped_w<0x01000000>));
}

void rosco_m68k_030_state::delegated_mem_map(address_map &map)
{
	map(0x000000, 0x000007).view(m_bootvect);
	m_bootvect[0](0x000000, 0x000007).rom().region("monitor", 0);                // After first write we act as RAM
	m_bootvect[0](0x000000, 0x000007).w(FUNC(rosco_m68k_030_state::bootvect_w)); // ROM mirror just during reset

	// Additional address space to bus error
	map(0x01000000, 0xfffffdff).rw(FUNC(rosco_m68k_030_state::unmapped_r<0x01000000>), FUNC(rosco_m68k_030_state::unmapped_w<0x01000000>));
}

void rosco_m68k_state::cpu_space_map(address_map &map)
{
	delegated_cpu_space_map(map);
}

void rosco_m68k_000_state::delegated_cpu_space_map(address_map &map)
{
	map(0x00fffff0, 0x00ffffff).m(m_maincpu, FUNC(m68000_base_device::autovectors_map));
	map(0x00fffff9, 0x00fffff9).r(m_duart, FUNC(xr68c681_device::get_irq_vector));
}

void rosco_m68k_010_state::delegated_cpu_space_map(address_map &map)
{
	map(0x00fffff0, 0x00ffffff).m(m_maincpu, FUNC(m68000_base_device::autovectors_map));
	map(0x00fffff9, 0x00fffff9).r(m_duart, FUNC(xr68c681_device::get_irq_vector));
}

void rosco_m68k_020_state::delegated_cpu_space_map(address_map &map)
{
	map(0xfffffff0, 0xffffffff).m(m_maincpu, FUNC(m68000_base_device::autovectors_map));
	map(0xfffffff9, 0xfffffff9).r(m_duart, FUNC(xr68c681_device::get_irq_vector));
}

void rosco_m68k_030_state::delegated_cpu_space_map(address_map &map)
{
	map(0xfffffff0, 0xffffffff).m(m_maincpu, FUNC(m68000_base_device::autovectors_map));
	map(0xfffffff9, 0xfffffff9).r(m_duart, FUNC(xr68c681_device::get_irq_vector));
}

void rosco_m68k_state::machine_start()
{
	m_bus_error_timer = timer_alloc(FUNC(rosco_m68k_state::bus_error), this);

	// Expansion RAM, contiguous with the on-board 1MB (the first 1MB of
	// the RAM device is unused: the on-board RAM is the "sysram" share).
	if (m_ram->size() > 0x100000)
		m_maincpu->space(AS_PROGRAM).install_ram(0x100000, m_ram->size() - 1, m_ram->pointer() + 0x100000);

	save_item(NAME(m_bus_error));
}

void rosco_m68k_state::machine_reset()
{
	m_sdcard->spi_clock_w(CLEAR_LINE);

	bootvec_reset();
}

void rosco_m68k_000_state::bootvec_reset()
{
	// Reset pointer to bootvector in ROM for bootvector view
	m_bootvect.select(0);
}

void rosco_m68k_010_state::bootvec_reset()
{
	// Reset pointer to bootvector in ROM for bootvector view
	m_bootvect.select(0);
}

void rosco_m68k_020_state::bootvec_reset()
{
	// Reset pointer to bootvector in ROM for bootvector view
	m_bootvect.select(0);
}

void rosco_m68k_030_state::bootvec_reset()
{
	// Reset pointer to bootvector in ROM for bootvector view
	m_bootvect.select(0);
}

// Boot vector handlers: The PCB hardwires the first 8 bytes from 0x008000 to 0x0 at reset.
void rosco_m68k_000_state::bootvect_w(offs_t offset, uint16_t data, uint16_t mem_mask)
{
	COMBINE_DATA(&m_sysram[offset]);
	m_bootvect.disable(); // redirect all upcoming accesses to masking RAM until reset.
}

void rosco_m68k_010_state::bootvect_w(offs_t offset, uint16_t data, uint16_t mem_mask)
{
	COMBINE_DATA(&m_sysram[offset]);
	m_bootvect.disable(); // redirect all upcoming accesses to masking RAM until reset.
}

void rosco_m68k_020_state::bootvect_w(offs_t offset, uint32_t data, uint32_t mem_mask)
{
	COMBINE_DATA(&m_sysram[offset]);
	m_bootvect.disable(); // redirect all upcoming accesses to masking RAM until reset.
}

void rosco_m68k_030_state::bootvect_w(offs_t offset, uint32_t data, uint32_t mem_mask)
{
	COMBINE_DATA(&m_sysram[offset]);
	m_bootvect.disable(); // redirect all upcoming accesses to masking RAM until reset.
}

// Unmapped RAM and absent expansion devices both assert BERR. A byte access
// to an odd address only drives the low data lines.
template <offs_t Base>
uint16_t rosco_m68k_state::unmapped_r(offs_t offset, uint16_t mem_mask)
{
	set_bus_error(Base + (offset << 1) + (ACCESSING_BITS_8_15 ? 0 : 1), true);
	return 0xff;
}

template <offs_t Base>
void rosco_m68k_state::unmapped_w(offs_t offset, uint16_t data, uint16_t mem_mask)
{
	set_bus_error(Base + (offset << 1) + (ACCESSING_BITS_8_15 ? 0 : 1), false);
}

TIMER_CALLBACK_MEMBER(rosco_m68k_state::bus_error)
{
	m_bus_error = false;
}

void rosco_m68k_state::set_bus_error(uint32_t address, bool read)
{
	if (m_bus_error)
		return;

	if (machine().side_effects_disabled())
		return;

	m_bus_error = true;

	signal_bus_error(address, read);

	m_bus_error_timer->adjust(m_maincpu->cycles_to_attotime(16)); // let rmw cycles complete
}

void rosco_m68k_musashi_state::signal_bus_error(uint32_t address, bool read)
{
	m68000_musashi_device *const cpu = downcast<m68000_musashi_device *>(m_maincpu.target());

	cpu->set_buserror_details(address, read ? 1 : 0, cpu->get_fc()); // 1 = read
	cpu->set_input_line(M68K_LINE_BUSERROR, ASSERT_LINE);
	cpu->set_input_line(M68K_LINE_BUSERROR, CLEAR_LINE);
}

void rosco_m68k_000_state::signal_bus_error(uint32_t address, bool read)
{
	// The 68000 core tracks the faulting access itself, it just needs to be
	// told to abort the access in progress.
	downcast<m68000_device *>(m_maincpu.target())->trigger_bus_error();
}


/*
 * ROM definitions
 *
 * The rosco_m68k firmware is built from source by its users rather than
 * being a fixed mask ROM dump, so the hashes below simply describe the
 * build that ships in roms/.  To run a firmware you built yourself, drop
 * it in as rosco_m68k.rom and start with -novalidate or update the length
 * and hashes here to match.
 *
 * -bios v242 selects firmware 2.42 (release/version-2.42 of rosco_m68k,
 * code/firmware/rosco_m68k_firmware, "make rosco_m68k.rom", m68k-elf-gcc
 * 13.3 in the roscopeco/rosco_m68k image), as on a Classic v2 board in
 * use: compare a port's behaviour against the real firmware.
 */
ROM_START( rosco_m68k_000 )
	ROM_REGION16_BE(0x100000, "monitor", 0)
	ROM_DEFAULT_BIOS("v250")
	ROM_SYSTEM_BIOS(0, "v250", "Firmware 2.50.DEV")
	ROMX_LOAD( "rosco_m68k.rom", 0x00000, 0x0da000, CRC(13eb1ac1) SHA1(b62d386fa662ac7af3ac5cf472071e0502b88413), ROM_BIOS(0) )
	ROM_SYSTEM_BIOS(1, "v242", "Firmware 2.42")
	ROMX_LOAD( "rosco_m68k_242.rom", 0x00000, 0x00ab10, CRC(86950afb) SHA1(6ff2a6584b2f504e73428ffae2761deecbcffb5b), ROM_BIOS(1) )
ROM_END

ROM_START( rosco_m68k_010 )
	ROM_REGION16_BE(0x100000, "monitor", 0)
	ROM_DEFAULT_BIOS("v250")
	ROM_SYSTEM_BIOS(0, "v250", "Firmware 2.50.DEV")
	ROMX_LOAD( "rosco_m68k.rom", 0x00000, 0x0da000, CRC(13eb1ac1) SHA1(b62d386fa662ac7af3ac5cf472071e0502b88413), ROM_BIOS(0) )
	ROM_SYSTEM_BIOS(1, "v242", "Firmware 2.42")
	ROMX_LOAD( "rosco_m68k_242.rom", 0x00000, 0x00ab10, CRC(86950afb) SHA1(6ff2a6584b2f504e73428ffae2761deecbcffb5b), ROM_BIOS(1) )
ROM_END

ROM_START( rosco_m68k_020 )
	ROM_REGION32_BE(0x100000, "monitor", 0)
	ROM_DEFAULT_BIOS("v250")
	ROM_SYSTEM_BIOS(0, "v250", "Firmware 2.50.DEV")
	ROMX_LOAD( "rosco_m68k.rom", 0x00000, 0x0da000, CRC(13eb1ac1) SHA1(b62d386fa662ac7af3ac5cf472071e0502b88413), ROM_BIOS(0) )
	ROM_SYSTEM_BIOS(1, "v242", "Firmware 2.42")
	ROMX_LOAD( "rosco_m68k_242.rom", 0x00000, 0x00ab10, CRC(86950afb) SHA1(6ff2a6584b2f504e73428ffae2761deecbcffb5b), ROM_BIOS(1) )
ROM_END

ROM_START( rosco_m68k_030 )
	ROM_REGION32_BE(0x100000, "monitor", 0)
	ROM_DEFAULT_BIOS("v250")
	ROM_SYSTEM_BIOS(0, "v250", "Firmware 2.50.DEV")
	ROMX_LOAD( "rosco_m68k.rom", 0x00000, 0x0da000, CRC(13eb1ac1) SHA1(b62d386fa662ac7af3ac5cf472071e0502b88413), ROM_BIOS(0) )
	ROM_SYSTEM_BIOS(1, "v242", "Firmware 2.42")
	ROMX_LOAD( "rosco_m68k_242.rom", 0x00000, 0x00ab10, CRC(86950afb) SHA1(6ff2a6584b2f504e73428ffae2761deecbcffb5b), ROM_BIOS(1) )
ROM_END

} // anonymous namespace


/* Driver */
/*    YEAR  NAME            PARENT  COMPAT  MACHINE         INPUT       CLASS                 INIT        COMPANY                          FULLNAME                     FLAGS */
COMP( 2022, rosco_m68k_000, 0,      0,      rosco_m68k_000, rosco_m68k, rosco_m68k_000_state, empty_init, "The Really Old-School Company", "rosco_m68k Classic V2 000", MACHINE_NO_SOUND_HW )
COMP( 2022, rosco_m68k_010, 0,      0,      rosco_m68k_010, rosco_m68k, rosco_m68k_010_state, empty_init, "The Really Old-School Company", "rosco_m68k Classic V2 010", MACHINE_NO_SOUND_HW )
COMP( 2023, rosco_m68k_020, 0,      0,      rosco_m68k_020, rosco_m68k, rosco_m68k_020_state, empty_init, "The Really Old-School Company", "rosco_m68k Classic V2 020", MACHINE_NO_SOUND_HW )
COMP( 2023, rosco_m68k_030, 0,      0,      rosco_m68k_030, rosco_m68k, rosco_m68k_030_state, empty_init, "The Really Old-School Company", "rosco_m68k Classic V2 030", MACHINE_NO_SOUND_HW )
