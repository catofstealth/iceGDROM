SECTIONS {
	/DISCARD/ : {
		*(.comment)
		*(.riscv.attributes)
		*(.debug*)
	}
	.memory : {
		. = 0x000000;
		start.o(.text);
		*(.sdata*);
		*(.sbss*);
                *(.data*);
		*(.rodata*);
		*(.text*);
		*(*);
		. = ALIGN(4);
		end = .;
	}
	ASSERT(end <= __stack - 500, "Less than 500 left for stack!")
}
