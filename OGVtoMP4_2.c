#include <glib.h>
#include <gst/gst.h>

/*
Añado videoconvert, crea un fichero pero no se puede leer, los frames no son validos

*/

static gboolean bus_callback (GstBus *bus, GstMessage *msg, gpointer data){

	GMainLoop *loop = (GMainLoop *) data;
	switch (GST_MESSAGE_TYPE (msg)) {

		case GST_MESSAGE_EOS:
			g_print ("Fin del stream\n");
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

GstElement *pipeline, *video;

static void cb_newpad (GstElement *decodebin, GstPad *pad, gpointer data){

	GstCaps *caps;
	GstStructure *str;
	GstPad *videopad;

	g_print ("Recibido un pad nuevo '%s' de '%s':\n", GST_PAD_NAME (pad), GST_ELEMENT_NAME (decodebin));

	/* Si ya esta enlazado no se hace nada */
	videopad = gst_element_get_static_pad (video, "ghost_pad_sink");
	if (GST_PAD_IS_LINKED (videopad)) {
		g_object_unref (videopad);
		return;
	}
	
	/* comprueba el pad que sea de video */
	caps = gst_pad_query_caps (pad, NULL);
	str = gst_caps_get_structure (caps, 0);
	
	if (!g_strrstr (gst_structure_get_name (str), "video")) {
		gst_caps_unref (caps);
		gst_object_unref (videopad);
		return;
	}

	gst_caps_unref (caps);

	
	/* Enlaza el decodebin con videoconvert  */
	gst_pad_link (pad, videopad);
	g_object_unref (videopad);
}


gint main (gint argc, gchar *argv[]){

	GMainLoop *loop;
	GstElement *src, *dec, *conv, *sink, *codec;
	GstPad *videopad;
	GstBus *bus;
	
	/* Inicio libreria GStreamer */
	gst_init (&argc, &argv);
	loop = g_main_loop_new (NULL, FALSE);

	/* Chequeo argumentos */
	if (argc != 2) {
		g_print ("Uso: %s <filename>\n", argv[0]);
		return -1;
	}

	/* Crea el pipeline y escucha el bus */
	pipeline = gst_pipeline_new ("pipeline");
	bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
	gst_bus_add_watch (bus, bus_callback, loop);
	gst_object_unref (bus);

	src = gst_element_factory_make ("filesrc", "source");
	g_object_set (G_OBJECT (src), "location", argv[1], NULL);

	dec = gst_element_factory_make ("decodebin", "decoder");
	g_signal_connect (dec, "pad-added", G_CALLBACK (cb_newpad), NULL);

	gst_bin_add_many (GST_BIN (pipeline), src, dec, NULL);
	gst_element_link (src, dec);

	/* ---------------------pipeline---------------------------------------*/
	/* 			   ghost-pad-sink->videobin--------------------*/
	/* filesrc -> decodebin -> (videopad)videoconvert-->x264enc-->filesink */

	/* crea la parte de salida */
	video = gst_bin_new ("videobin");
	conv = gst_element_factory_make ("videoconvert", "vconv");
	videopad = gst_element_get_static_pad (conv, "sink");
	
	codec = gst_element_factory_make ("x264enc", "codec");
	g_object_set (G_OBJECT(codec), "byte-stream", TRUE, NULL);
	sink = gst_element_factory_make ("filesink", "sink");
	g_object_set (G_OBJECT(sink), "location","./salida12345.mp4", NULL);

	gst_bin_add_many (GST_BIN (video), conv, codec, sink, NULL);
	gst_element_link_many (conv, codec, sink, NULL);
	
	/* Crea un ghost-pad en el sink del convertidor */
	gst_element_add_pad (video, gst_ghost_pad_new ("ghost_pad_sink", videopad));

	gst_object_unref (videopad);

	/* Completamos la tuberia */
	gst_bin_add (GST_BIN (pipeline), video);

	/* Estado --> PLAYING */
	gst_element_set_state (pipeline, GST_STATE_PLAYING);
	g_main_loop_run (loop);

	/* salida ordenada */
	gst_element_set_state (pipeline, GST_STATE_NULL);
	gst_object_unref (GST_OBJECT (pipeline));

return 0;
}

