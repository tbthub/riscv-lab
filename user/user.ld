OUTPUT_ARCH( "riscv" )

/* 定义程序头表 */
PHDRS
{
    text PT_LOAD FLAGS(0x5); /* 101 */
    rodata PT_LOAD FLAGS(0x4); /* 100 */
    eh_frame PT_NULL FLAGS(0x4); /* 100 */
    data PT_LOAD FLAGS(0x6); /* 110 */
    bss PT_LOAD FLAGS(0x6); /* 110 */
}

SECTIONS
{
 . = 0x200000000;
 
  .text : {
    *(.text .text.*)
  } :text

  .rodata : {
    . = ALIGN(16);
    *(.srodata .srodata.*) /* do not need to distinguish this from .rodata */
    . = ALIGN(16);
    *(.rodata .rodata.*)
  } :rodata

  .eh_frame : {
       *(.eh_frame)
       *(.eh_frame.*)
   } :eh_frame

  . = ALIGN(0x1000);
  .data : {
    . = ALIGN(16);
    *(.sdata .sdata.*) /* do not need to distinguish this from .data */
    . = ALIGN(16);
    *(.data .data.*)
  } :data

  .bss : {
    . = ALIGN(16);
    *(.sbss .sbss.*) /* do not need to distinguish this from .bss */
    . = ALIGN(16);
    *(.bss .bss.*)
  } :bss

  PROVIDE(end = .);
}
