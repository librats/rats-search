import __ from '../../app/translation'

export const editMenuTemplateFunc = () => ({
	label: __("Edit"),
	submenu: [
		{ label: __("Undo"), accelerator: "CmdOrCtrl+Z", selector: "undo:" },
		{ label: __("Redo"), accelerator: "Shift+CmdOrCtrl+Z", selector: "redo:" },
		{ type: "separator" },
		{ label: __("Cut"), accelerator: "CmdOrCtrl+X", selector: "cut:" },
		{ label: __("Copy"), accelerator: "CmdOrCtrl+C", selector: "copy:" },
		{ label: __("Paste"), accelerator: "CmdOrCtrl+V", selector: "paste:" },
		{ label: __("Select All"), accelerator: "CmdOrCtrl+A", selector: "selectAll:" }
	]
});
