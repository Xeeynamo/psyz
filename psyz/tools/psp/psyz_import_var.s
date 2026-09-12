/*
 * psyz_import_var.s - IMPORT_VAR: variable-import stub for PSP PRX modules.
 *
 * Inspired from PSPSDK's IMPORT_START/IMPORT_FUNC macros.
 */

.macro IMPORT_VAR module, varid, varname

	.set push

	.extern __stub_module_\module

	.section .sceVstub, "aw", @progbits
	.globl  \varname
	.type   \varname, @object
\varname:
	.word   0
	.size   \varname, 4

	.section .rodata.sceVstubNid, "a"
	.word   \varid

	.set pop
.endm
