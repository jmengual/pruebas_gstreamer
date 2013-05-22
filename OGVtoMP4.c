#include <gst/gst.h>
#include <glib.h>

/*
No funciona.
Intenta convertir archivo de video que se pasa por argumento en formato mp4 con codec x264	 
*/

static gboolean bus_callback (GstBus *bus, GstMessage *msg, gpointer data){

	GMainLoop *loop = (GMainLoop *) data;
	switch (GST_MESSAGE_TYPE (msg)) {

		case GST_MESSAGE_EOS:
			g_print ("Fin del flujo\n");
			g_main_loop_quit (loop);
			break;

		case GST_MESSAGE_ERROR: {
			gchar *debug;
			GError *error;
			gst_message_parse_error (msg, &error, &debug);
			g_free (debug);
			g_printerr ("Error: %s\n", error->message);
			g_error_free (error);
			g_main_loop_quit (loop);
			break;
		}

		default:
			break;
	}
 return TRUE;
}

   
/* Estructura que contiene los elementos del bin */
typedef struct _CustomData {
  GstElement *pipeline;
  GstElement *source;
  GstElement* decode;
  GstElement* codec;
  GstElement* filtro;
  GstElement *sink;
} CustomData;
   
/* Manejador de la senyal "pad-added" de decodebin */
static void manejador_pad_added (GstElement *src, GstPad *new_pad, CustomData *data) {

	GstPad *sink_pad;
	GstPadLinkReturn ret;
	GstCaps *new_pad_caps = NULL;
	GstStructure *new_pad_struct = NULL;
	const gchar *new_pad_type = NULL;
   
	g_print ("Recibido un pad nuevo '%s' de '%s':\n", GST_PAD_NAME (new_pad), GST_ELEMENT_NAME (src));

	/* Solicita sink pad del codecX264 */  
	sink_pad = gst_element_get_static_pad (data->codec, "sink"); 
	/* Si ya esta enlazado salimos. */
	if (gst_pad_is_linked (sink_pad)) {
		g_print ("  Ya esta enlazado el pad. Saliendo del manejador.\n");
		g_object_unref (sink_pad);
    		return;
  	}
   
	/* Se comprueba el tipo de pad. Buscamos el video unicamente */
  	
	new_pad_caps = gst_pad_query_caps (new_pad, NULL);
 	new_pad_struct = gst_caps_get_structure (new_pad_caps, 0);
 	new_pad_type = gst_structure_get_name (new_pad_struct);
 	if (!g_str_has_prefix (new_pad_type, "video/x-raw")) {
 		g_print ("  Pad de tipo '%s' , no es raw video. Se ignora.\n", new_pad_type);
		g_object_unref (sink_pad); 
		g_object_unref(new_pad_caps); /* Aqui habria que chequear que no estan a NULL */
		g_object_unref(new_pad_struct);
    		return;
  	}
   
 	/* Intentado enlazar src-pad de decodebin con sink-pad de encX264 */
 	ret = gst_pad_link (new_pad, sink_pad);
 	if (GST_PAD_LINK_FAILED (ret)) {
 		g_print (" El tipo de pad es '%s' pero fallo al intentar enlazar.\n", new_pad_type);
 	} else {
		g_print (" Enlazado con exito (tipo '%s').\n", new_pad_type);
	}
   
	if (new_pad_caps != NULL)
		gst_caps_unref (new_pad_caps);
   
	/* libera sink-pad */
  	gst_object_unref (sink_pad);
}
   
int main(int argc, char *argv[]) {

	GMainLoop *loop;
	CustomData data;
	GstBus *bus;
	GstStateChangeReturn ret;
	GstCaps* caps;
   
 	/* Inicializa libreria GStreamer y crea el hilo principal */
 	gst_init (&argc, &argv);
	loop = g_main_loop_new (NULL, FALSE);

   
	/* Creacion de los elementos */
	data.source = gst_element_factory_make ("filesrc", "source");
	data.decode = gst_element_factory_make ("decodebin", "decode");
	data.codec = gst_element_factory_make ("x264enc", "codec");
	data.filtro = gst_element_factory_make ("capsfilter", "filtro");
	data.sink = gst_element_factory_make ("filesink", "sink");

	/* Creacion de la tuberia y conexion del bus al manejador de bus */
	data.pipeline = gst_pipeline_new ("test-pipeline");
	bus = gst_pipeline_get_bus (GST_PIPELINE (data.pipeline));
	gst_bus_add_watch (bus, bus_callback, loop);
	gst_object_unref (bus);

	if (!data.pipeline || !data.source || !data.decode || !data.codec || !data.filtro || !data.sink) {
		g_printerr ("Fallo al crear algun elemento.\n");
		return -1;
	}
   
	/* Enlace source-->decode */
	if (!gst_element_link (data.source, data.decode)) {
		g_printerr ("Fallo al enlazar source-->decode.\n");
		gst_object_unref (data.pipeline);
		return -1;
	}

	/* Configura source */   
	g_object_set (G_OBJECT (data.source), "location", argv[1], NULL);
	/* Configura el codec y filtro */
	g_object_set (G_OBJECT(data.codec), "byte-stream", TRUE, NULL);
	
	caps = gst_caps_from_string( "video/x-h264,framerate=25/1,stream-format=byte-stream,alignment=au");
	g_object_set(G_OBJECT(data.filtro), "caps", caps, NULL);

	/* Configura el elemento sink, crea un archivo de nombre salida123.mp4 */
	g_object_set (G_OBJECT(data.sink), "location","./salida123.mp4", NULL);

	/* Conexion del manejador a la senyal pad-added del elemento decodebin */
	g_signal_connect (data.decode, "pad-added", G_CALLBACK (manejador_pad_added), &data);

	/* Anyadimos todos los elementos a la tuberia */
	gst_bin_add_many (GST_BIN (data.pipeline), data.source, data.decode, data.codec, data.filtro, data.sink, NULL);


	/* Enlace codec-->filtro-->sink */
	if (!gst_element_link_many (data.codec, data.filtro, data.sink, NULL)) {
		g_printerr ("Fallo al enlazar codec-->filtro-->sink.\n");
		gst_object_unref (data.pipeline);
		return -1;
	}
	
	g_print ("Construido el pipeline\n");   
	
	/* Se pone la tuberia al estado PLAYING */
	ret = gst_element_set_state (data.pipeline, GST_STATE_PLAYING);
	if (ret == GST_STATE_CHANGE_FAILURE) {
		g_printerr ("Error. Imposible poner la tuberia al estado PLAYING.\n");
		gst_object_unref (data.pipeline);
		return -1;
	}
   
	/* Libera recursos y salida ordenada */
	gst_object_unref (bus);
	gst_element_set_state (data.pipeline, GST_STATE_NULL);
	gst_object_unref (data.pipeline);
	return 0;
}
   



