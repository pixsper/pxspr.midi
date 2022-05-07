#include <memory>
#include <vector>
#include <regex>

#ifdef WIN_VERSION
#include <Windows.h>
#endif

#include <RtMidi.h>

#include "ext.h"
#include "ext_obex.h"
#include "version.h"


typedef struct _pxspr_midiout
{
	t_object object;
	void* status_outlet;

	RtMidiOut* rt_midi_out;
	std::vector<std::string>* ports;
	std::vector<t_uint8>* output_buffer;

	t_atom_long active_port_index;

	t_bool is_port_open;
	
} t_pxspr_midiout;


BEGIN_USING_C_LINKAGE

void *midiout_new(t_symbol *s, long argc, t_atom *argv);
void midiout_free(t_pxspr_midiout *x);
void midiout_assist(t_pxspr_midiout *x, void *b, long m, long a, char *s);
void midiout_dblclick(t_pxspr_midiout *x);

void midiout_enumerateports(t_pxspr_midiout *x);
void midiout_openportname(t_pxspr_midiout *x, t_symbol* portName);
void midiout_openport(t_pxspr_midiout *x, unsigned int portIndex);
void midiout_closeport(t_pxspr_midiout *x);

void midiout_int(t_pxspr_midiout* x, t_atom_long value);
void midiout_float(t_pxspr_midiout* x, double value);
void midiout_list(t_pxspr_midiout* x, t_symbol* s, long argc, t_atom *argv);
void midiout_port(t_pxspr_midiout* x, t_symbol* s, long argc, t_atom *argv);
void midiout_anything(t_pxspr_midiout* x, t_symbol* s, long argc, t_atom *argv);

END_USING_C_LINKAGE

t_symbol* _sym_port_open;

t_class* pxspr_midiout_class;


void C74_EXPORT ext_main(void* r)
{	
	t_class* c = class_new("pxspr.midiout", (method)midiout_new, (method)midiout_free,
	                       (long)sizeof(t_pxspr_midiout), 0L, A_GIMME, 0);

	class_addmethod(c, (method)midiout_assist,			"assist",		    A_CANT,  0);
	class_addmethod(c, (method)midiout_dblclick,		"dblclick",		A_CANT,	 0);
	class_addmethod(c, (method)midiout_int,				"int",			A_LONG,  0);
	class_addmethod(c, (method)midiout_float,			"float",		    A_FLOAT, 0);
	class_addmethod(c, (method)midiout_list,			"list",			A_GIMME, 0);
	class_addmethod(c, (method)midiout_port,            "port",           A_GIMME, 0);
	class_addmethod(c, (method)midiout_anything,		"anything",		A_GIMME, 0);

	CLASS_ATTR_CHAR(c, "port_open", ATTR_SET_OPAQUE_USER, t_pxspr_midiout, is_port_open);
	CLASS_ATTR_LABEL(c, "port_open", 0, "Port Open");

	class_register(CLASS_BOX, c);
	pxspr_midiout_class = c;

	_sym_port_open = gensym("port_open");

#ifdef NDEBUG
	object_post(nullptr, "pxspr.midi V%ld.%ld.%ld - %s",
				PXSPR_MIDI_VERSION_MAJOR, PXSPR_MIDI_VERSION_MINOR, PXSPR_MIDI_VERSION_BUGFIX,
				PXSPR_MIDI_COPYRIGHT);
#else
	object_post(nullptr, "pxspr.midi V%ld.%ld.%ld %s - DEBUG - %s",
	            PXSPR_MIDI_VERSION_MAJOR, PXSPR_MIDI_VERSION_MINOR, PXSPR_MIDI_VERSION_BUGFIX,
	            PXSPR_MIDI_VERSION_SHA, PXSPR_MIDI_COPYRIGHT);
#endif
}

void* midiout_new(t_symbol *s, long argc, t_atom *argv)
{
	t_pxspr_midiout *x = nullptr;

	x = (t_pxspr_midiout*)object_alloc(pxspr_midiout_class);

	if (x == nullptr)
		return nullptr;

	x->status_outlet = intout(x);
	
	x->rt_midi_out = new RtMidiOut(RtMidi::UNSPECIFIED, "pxspr.midiout");
	x->ports = new std::vector<std::string>();

	x->output_buffer = new std::vector<t_uint8>(1);

	x->active_port_index = -1;

	x->is_port_open = false;

	midiout_enumerateports(x);

	if (argc > 0)
	{
		if (atom_gettype(argv) == A_SYM)
			midiout_openportname(x, atom_getsym(argv));
	}

	if (x->active_port_index == -1 && x->ports->size() > 0)
		midiout_openport(x, 0);

	return x;
}

void midiout_free(t_pxspr_midiout *x)
{
	midiout_closeport(x);

	delete x->rt_midi_out;
	delete x->ports;
	delete x->output_buffer;
}

void midiout_assist(t_pxspr_midiout *x, void *b, long m, long a, char *s)
{
	const char* inlet_label = "Raw MIDI Messages";
	const char* status_outlet_label = "Status of Port";

	if (m == ASSIST_INLET)
		strncpy(s, inlet_label, strlen(inlet_label) + 1);
	else
		strncpy(s, status_outlet_label, strlen(status_outlet_label) + 1);
}

void midiout_dblclick(t_pxspr_midiout *x)
{
	t_atom_long selected_port_index = -1;

#ifdef WIN_VERSION

	HWND wnd = main_get_client();
	HMENU menu = CreatePopupMenu();

	for (unsigned int i = 0; i < x->ports->size(); ++i)
		AppendMenu(menu, MF_STRING, i + 1, (*x->ports)[i].c_str());

	if (x->activePortIndex != -1)
		CheckMenuItem(menu, x->active_port_index + 1, MF_BYCOMMAND | MF_CHECKED);

	POINT cursor;
	GetCursorPos(&cursor);

	selected_port_index = TrackPopupMenuEx(menu, TPM_RETURNCMD, cursor.x - 50, cursor.y, wnd, nullptr) - 1;

#elif MAC_VERSION

	// TODO: Add handling for this
	selected_port_index = -1;

#endif

	if (selected_port_index != -1 && selected_port_index != x->active_port_index)
		midiout_openport(x, selected_port_index);
}


void midiout_enumerateports(t_pxspr_midiout *x)
{
	x->ports->clear();

	unsigned int port_count = x->rt_midi_out->getPortCount();

	x->ports->reserve(port_count);

	for (unsigned int i = 0; i < port_count; ++i)
		x->ports->push_back(x->rt_midi_out->getPortName(i));
}

void midiout_openportname(t_pxspr_midiout *x, t_symbol* portName)
{
	if (std::string(portName->s_name) == "None")
	{
		midiout_closeport(x);
		return;
	}

	auto result = find(x->ports->begin(), x->ports->end(), portName->s_name);

	if (result != x->ports->end())
		midiout_openport(x, result - x->ports->begin());
}

void midiout_openport(t_pxspr_midiout *x, unsigned int portIndex)
{
	midiout_closeport(x);

	if (portIndex == -1)
	{
		x->active_port_index = -1;
		object_attr_setchar(x, _sym_port_open, false);
	}
	else
	{
		try
		{
			x->rt_midi_out->openPort(portIndex);
			x->active_port_index = portIndex;
			object_attr_setchar(x, _sym_port_open, true);
		}
		catch (...)
		{
			object_error((t_object*)x, "Failed to open port '%s', port may already be in use by Max. To use disable this output port in Max's MIDI settings.", (*x->ports)[portIndex].c_str());
			x->active_port_index = -1;
			object_attr_setchar(x, _sym_port_open, false);
		}
	}

	outlet_int(x->status_outlet, x->is_port_open ? 1 : 0);
}

void midiout_closeport(t_pxspr_midiout *x)
{
	if (!x->rt_midi_out->isPortOpen())
		return;

	x->rt_midi_out->closePort();
	x->active_port_index = -1;
	object_attr_setchar(x, _sym_port_open, false);

	outlet_int(x->status_outlet, x->is_port_open ? 1 : 0);
}


void midiout_int(t_pxspr_midiout* x, t_atom_long value)
{
	x->output_buffer->at(0) = value;
	x->rt_midi_out->sendMessage(x->output_buffer);
}

void midiout_float(t_pxspr_midiout* x, double value)
{
	midiout_int(x, (t_atom_long)value);
}

void midiout_list(t_pxspr_midiout* x, t_symbol* s, long argc, t_atom *argv)
{
	std::vector<t_uint8> buffer(argc);

	for (int i = 0; i < argc; ++i)
	{
		switch (atom_gettype(argv + i))
		{
		case A_LONG:
			buffer[i] = atom_getlong(argv + i);
			break;
		default:
			object_error((t_object*)x, "Element of input list has invalid type");
			return;
		}
	}

	x->rt_midi_out->sendMessage(&buffer);
}

void midiout_port(t_pxspr_midiout* x, t_symbol* s, long argc, t_atom *argv)
{
	if (atom_gettype(argv) == A_SYM)
		midiout_openportname(x, atom_getsym(argv));
}

void midiout_anything(t_pxspr_midiout* x, t_symbol* s, long argc, t_atom *argv)
{
	midiout_openportname(x, s);
}