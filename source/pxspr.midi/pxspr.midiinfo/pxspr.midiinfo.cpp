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


typedef struct _midiinfo
{
	t_object object;            // The object
	void* outlet;
	
	long inlet_index;
	void* proxy;

	RtMidiIn* rt_midi_in;
	std::vector<std::string>* input_ports;
	
	RtMidiOut* rt_midi_out;
	std::vector<std::string>* output_ports;
	
} t_midiinfo;


BEGIN_USING_C_LINKAGE

void *midiinfo_new(t_symbol *s, long argc, t_atom *argv);
void midiinfo_free(t_midiinfo *x);
void midiinfo_assist(t_midiinfo *x, void *b, long m, long a, char *s);

void midiinfo_enumerateports(t_midiinfo *x);
void midiinfo_bang(t_midiinfo *x);
void midiinfo_int(t_midiinfo *x, t_atom_long value);
void midiinfo_controllers(t_midiinfo *x, t_symbol *s, long argc, t_atom *argv);
void midiinfo_outputports(t_midiinfo *x, bool isInputs);

END_USING_C_LINKAGE

t_class *pxspr_midiinfo_class;


void C74_EXPORT ext_main(void* r)
{
	common_symbols_init();
	
	t_class* c = class_new("pxspr.midiinfo", (method)midiinfo_new, (method)midiinfo_free,
						   (long)sizeof(t_midiinfo), 0L, A_GIMME, 0);

	class_addmethod(c, (method)midiinfo_assist,			"assist",		  A_CANT,  0);
	class_addmethod(c, (method)midiinfo_bang,			"bang",                  0);
	class_addmethod(c, (method)midiinfo_int,			"int",          A_LONG,  0);
	class_addmethod(c, (method)midiinfo_controllers,	"controllers",  A_GIMME, 0);
	
	class_register(CLASS_BOX, c);
	pxspr_midiinfo_class = c;

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

void midiinfo_assist(t_midiinfo *x, void *b, long m, long a, char *s)
{
	const char* inlet_1_label = "bang, int List Outputs, int Sets Menu Item";
	const char* inlet_2_label = "bang, int List Inputs, int Sets Menu Item";
	const char* outlet_label = "Connect to menu Object";
	
	if (m == ASSIST_INLET)
	{
		if (a == 0)
			strncpy(s, inlet_1_label, strlen(inlet_1_label) + 1);
		else
			strncpy(s, inlet_2_label, strlen(inlet_2_label) + 1);
	}
	else
	{
		strncpy(s, outlet_label, strlen(outlet_label) + 1);
	}
}

void *midiinfo_new(t_symbol *s, long argc, t_atom *argv)
{
	t_midiinfo *x = nullptr;

	x = (t_midiinfo*)object_alloc(pxspr_midiinfo_class);

	if (x == nullptr)
		return nullptr;

	x->outlet = outlet_new(x, nullptr);
	x->proxy = proxy_new(x, 1, &x->inlet_index);

	x->rt_midi_in = new RtMidiIn(RtMidi::UNSPECIFIED, "pxspr.midiin");
	x->input_ports = new std::vector<std::string>();
	
	x->rt_midi_out = new RtMidiOut(RtMidi::UNSPECIFIED, "pxspr.midiout");
	x->output_ports = new std::vector<std::string>();

	midiinfo_enumerateports(x);

	return x;
}

void midiinfo_free(t_midiinfo *x)
{
	delete x->rt_midi_in;
	delete x->input_ports;
	delete x->rt_midi_out;
	delete x->output_ports;
	
	object_free(x->proxy);
}

void midiinfo_enumerateports(t_midiinfo *x)
{
	x->input_ports->clear();
	x->output_ports->clear();

	unsigned int inputPortCount = x->rt_midi_in->getPortCount();
	unsigned int outputPortCount = x->rt_midi_out->getPortCount();

	x->input_ports->reserve(inputPortCount);
	x->output_ports->reserve(outputPortCount);

#ifdef WIN_VERSION
	
	std::regex filter(R"( \d+$)");

	for (unsigned int i = 0; i < inputPortCount; ++i)
	{
		string name = x->rtMidiIn->getPortName(i);
		x->inputPorts->push_back(regex_replace(name, filter, ""));
	}
	
#elif MAC_VERSION

	for (unsigned int i = 0; i < inputPortCount; ++i)
		x->input_ports->push_back(x->rt_midi_in->getPortName(i));
	
#endif
	
	for (unsigned int i = 0; i < outputPortCount; ++i)
		x->output_ports->push_back(x->rt_midi_out->getPortName(i));
}

void midiinfo_bang(t_midiinfo *x)
{
	midiinfo_outputports(x, proxy_getinlet((t_object*)x) == 0);
}

void midiinfo_int(t_midiinfo *x, t_atom_long value)
{
	midiinfo_bang(x);
	
	if (value < 0)
		return;
	
	t_atom argv;
	atom_setlong(&argv, value);
	outlet_anything(x->outlet, _sym_set, 1, &argv);
}

void midiinfo_controllers(t_midiinfo *x, t_symbol *s, long argc, t_atom *argv)
{
	midiinfo_outputports(x, true);
	
	if (argc > 0)
	{
		if (atom_gettype(argv) == A_LONG)
		{
			if (atom_getlong(argv) > 0)
				outlet_anything(x->outlet, _sym_set, 1, argv);
		}
	}
	else
	{
		t_atom argv;
		atom_setlong(&argv, 0);
		outlet_anything(x->outlet, _sym_set, 1, &argv);
	}
}

void midiinfo_outputports(t_midiinfo *x, bool isInputs)
{
	t_atom argv;
	
	outlet_anything(x->outlet, _sym_clear, 0, nullptr);
	
	atom_setsym(&argv, gensym("None"));
	outlet_anything(x->outlet,_sym_append, 1, &argv);

	if (isInputs)
	{
		for(const std::string& port : *x->input_ports)
		{
			atom_setsym(&argv, gensym(port.c_str()));
			outlet_anything(x->outlet, _sym_append, 1, &argv);
		}
	}
	else
	{
		for(const std::string& port : *x->output_ports)
		{
			atom_setsym(&argv, gensym(port.c_str()));
			outlet_anything(x->outlet, _sym_append, 1, &argv);
		}
	}
}