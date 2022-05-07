#include <memory>
#include <vector>
#include <queue>
#include <mutex>
#include <regex>

#ifdef WIN_VERSION
#include <Windows.h>
#endif

#include <RtMidi.h>

#include "ext.h"
#include "ext_obex.h"
#include "version.h"


typedef struct _pxspr_sysexin
{
	t_object object;
	void* outlet;
	void* status_outlet;

	RtMidiIn* rt_midi_in;
	std::vector<std::string>* ports;

	std::queue<std::vector<t_uint8>>* input_queue;
	std::mutex* output_mutex;

	t_atom_long active_port_index;
    
	t_bool is_port_open;
	t_bool is_receiving_sysex;
	
} t_pxspr_sysexin;


BEGIN_USING_C_LINKAGE

void* sysexin_new(t_symbol *s, long argc, t_atom *argv);
void sysexin_free(t_pxspr_sysexin *x);
void sysexin_assist(t_pxspr_sysexin *x, void *b, long m, long a, char *s);
void sysexin_dblclick(t_pxspr_sysexin *x);

void sysexin_enumerateports(t_pxspr_sysexin *x);
void sysexin_openportname(t_pxspr_sysexin *x, t_symbol* portName);
void sysexin_openport(t_pxspr_sysexin *x, int portIndex);
void sysexin_closeport(t_pxspr_sysexin *x);
void sysexin_datareceivedcallback(double timeStamp, std::vector<t_uint8>* message, void* user_data);
void sysexin_outputdata(t_pxspr_sysexin *x, t_symbol* s, long argc, t_atom* argv);
void sysexin_port(t_pxspr_sysexin* x, t_symbol* s, long argc, t_atom *argv);
void sysexin_anything(t_pxspr_sysexin* x, t_symbol* s, long argc, t_atom *argv);

END_USING_C_LINKAGE

t_symbol* _sym_port_open;

t_class* pxspr_sysexin_class;

void C74_EXPORT ext_main(void* r)
{	
	t_class* c = class_new("pxspr.sysexin", (method)sysexin_new, (method)sysexin_free,
						   (long)sizeof(t_pxspr_sysexin),nullptr, A_GIMME, 0);

	class_addmethod(c, (method)sysexin_assist,			"assist",		    A_CANT,  0);
	class_addmethod(c, (method)sysexin_dblclick,		"dblclick",		A_CANT,  0);
	class_addmethod(c, (method)sysexin_port,            "port",           A_GIMME, 0);
	class_addmethod(c, (method)sysexin_anything,		"anything",		A_GIMME, 0);
	
	CLASS_ATTR_CHAR(c, "port_open", ATTR_SET_OPAQUE_USER, t_pxspr_sysexin, is_port_open);
	CLASS_ATTR_LABEL(c, "port_open", 0, "Port Open");

	class_register(CLASS_BOX, c);
	pxspr_sysexin_class = c;

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

void sysexin_assist(t_pxspr_sysexin *x, void *b, long m, long a, char *s)
{
	const char* inlet_label = "port Message Sets MIDI Input Port/Device";
	const char* outlet_label = "Raw MIDI Messages";
	const char* status_outlet_label = "Status of Port";
	
	if (m == ASSIST_INLET)
	{
		strncpy(s, inlet_label, strlen(inlet_label) + 1);
	}
	else
	{
		if (a == 0)
			strncpy(s, outlet_label, strlen(outlet_label) + 1);
		else
			strncpy(s, status_outlet_label, strlen(status_outlet_label) + 1);
	}
		
}

void *sysexin_new(t_symbol *s, long argc, t_atom *argv)
{
	t_pxspr_sysexin *x;

	x = (t_pxspr_sysexin*)object_alloc(pxspr_sysexin_class);

	if (x == nullptr)
		return nullptr;

	x->status_outlet = intout(x);
	x->outlet = intout(x);	

	x->rt_midi_in = new RtMidiIn(RtMidi::UNSPECIFIED,"pxspr.sysexin");
    
	x->rt_midi_in->setCallback(sysexin_datareceivedcallback, x);
    x->rt_midi_in->ignoreTypes(false, true, true);

	x->ports = new std::vector<std::string>();
	x->input_queue = new std::queue<std::vector<t_uint8>>();
	x->output_mutex = new std::mutex();

	x->active_port_index = -1;
    
	x->is_port_open = false;
    x->is_receiving_sysex = false;

	sysexin_enumerateports(x);
	
	if (argc > 0)
	{
		if (atom_gettype(argv) == A_SYM)
			sysexin_openportname(x, atom_getsym(argv));
	}

	return x;
}

void sysexin_free(t_pxspr_sysexin *x)
{
	sysexin_closeport(x);

	delete x->rt_midi_in;
	delete x->ports;
	delete x->input_queue;
	delete x->output_mutex;
}

void sysexin_dblclick(t_pxspr_sysexin *x)
{
	t_atom_long selected_port_index;

#ifdef WIN_VERSION

	HWND wnd = main_get_client();
	HMENU menu = CreatePopupMenu();

	for (unsigned int i = 0; i < x->ports->size(); ++i)
		AppendMenu(menu, MF_STRING, i + 1, (*x->ports)[i].c_str());

	if (x->activePortIndex != -1)
		CheckMenuItem(menu, x->activePortIndex + 1, MF_BYCOMMAND | MF_CHECKED);

	POINT cursor;
	GetCursorPos(&cursor);

	selectedPortIndex = TrackPopupMenuEx(menu, TPM_RETURNCMD, cursor.x - 50, cursor.y, wnd, nullptr) - 1;

#elif MAC_VERSION

    // TODO: Add handling for this
    selected_port_index = -1;

#endif

	if (selected_port_index != -1 && selected_port_index != x->active_port_index)
		sysexin_openport(x, selected_port_index);
}



void sysexin_enumerateports(t_pxspr_sysexin *x)
{
	x->ports->clear();

	unsigned int portCount = x->rt_midi_in->getPortCount();

	x->ports->reserve(portCount);

#ifdef WIN_VERSION
	
	std::regex filter(R"( \d+$)");

	for (unsigned int i = 0; i < portCount; ++i)
	{
		string name = x->rt_midi_in->getPortName(i);
		x->ports->push_back(std::regex_replace(name, filter, ""));
	}
	
#elif MAC_VERSION

	for (unsigned int i = 0; i < portCount; ++i)
		x->ports->push_back(x->rt_midi_in->getPortName(i));
	
#endif

	if (x->active_port_index == -1 && x->ports->size() > 0)
		sysexin_openport(x, 0);
}

void sysexin_openportname(t_pxspr_sysexin *x, t_symbol* portName)
{
	if (strcmp("None", portName->s_name) == 0)
	{
		sysexin_closeport(x);
		return;
	}

	auto result = find(x->ports->begin(), x->ports->end(), portName->s_name);

	if (result != x->ports->end())
		sysexin_openport(x, result - x->ports->begin());
}

void sysexin_openport(t_pxspr_sysexin *x, int portIndex)
{
	sysexin_closeport(x);

	if (portIndex == -1)
	{
		x->is_receiving_sysex = false;
		x->active_port_index = -1;
		object_attr_setchar(x, _sym_port_open, false);
	}
	else
	{
		try
		{
			x->rt_midi_in->openPort(portIndex);
			x->is_receiving_sysex = false;
			x->active_port_index = portIndex;
			object_attr_setchar(x, _sym_port_open, true);
		}
		catch (...)
		{
			object_error((t_object*)x, "Failed to open port '%s', port may already be in use by Max. To use disable this input port in Max's MIDI settings.", (*x->ports)[portIndex].c_str());
			x->is_receiving_sysex = false;
			x->active_port_index = -1;
			object_attr_setchar(x, _sym_port_open, false);
		}
	}

	outlet_int(x->status_outlet, x->is_port_open ? 1 : 0);
}

void sysexin_closeport(t_pxspr_sysexin *x)
{
	if (!x->rt_midi_in->isPortOpen())
		return;

	x->rt_midi_in->closePort();
	x->active_port_index = -1;
	x->is_receiving_sysex = false;
	object_attr_setchar(x, gensym("port_open"), false);

	outlet_int(x->status_outlet, x->is_port_open ? 1 : 0);
}

void sysexin_datareceivedcallback(double timeStamp, std::vector<t_uint8>* message, void* user_data)
{
	t_pxspr_sysexin* x = (t_pxspr_sysexin*)user_data;

	x->input_queue->push(*message);
	schedule_delay(user_data, (method)sysexin_outputdata, 0, nullptr, 0, nullptr);
}

void sysexin_outputdata(t_pxspr_sysexin *x, t_symbol* s, long argc, t_atom* argv)
{
	std::lock_guard<std::mutex> lock(*x->output_mutex);

	while(!x->input_queue->empty())
	{
		std::vector<t_uint8>& message = x->input_queue->front();

		for (t_uint8 b : message)
		{
			if (x->is_receiving_sysex)
			{
				outlet_int(x->outlet, b);
				if (b == 0xF7)
					x->is_receiving_sysex = false;
			}
			else if (b == 0xF0)
			{
				x->is_receiving_sysex = true;
				outlet_int(x->outlet, b);
			}
		}

		x->input_queue->pop();
	}
}

void sysexin_port(t_pxspr_sysexin* x, t_symbol* s, long argc, t_atom *argv)
{
	if (atom_gettype(argv) == A_SYM)
		sysexin_openportname(x, atom_getsym(argv));
}

void sysexin_anything(t_pxspr_sysexin* x, t_symbol* s, long argc, t_atom *argv)
{
	sysexin_openportname(x, s);
}