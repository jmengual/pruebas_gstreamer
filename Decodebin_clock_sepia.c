#include <glib.h>
#include <gst/gst.h>

static gboolean bus_callback (GstBus *bus, GstMessage *msg, gpointer data){

	GMainLoop *loop = (GMainLoop *) data;
	switch (GST_MESSAGE_TYPE (msg)) {

		case GST_MESSAGE_EOS:
			g_print ("End of stream\n");
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

	/* Antes de nada se comprueba que el ghostpad esta enlazado */
	videopad = gst_element_get_static_pad (video, "ghost_pad_sink");
	if (GST_PAD_IS_LINKED (videopad)) {
		g_object_unref (videopad);
		return;
	}
	
	/* comprueba que el pad creado es el de video */
	caps = gst_pad_query_caps (pad, NULL);
	str = gst_caps_get_structure (caps, 0);
	if (!g_strrstr (gst_structure_get_name (str), "video")) {
		gst_caps_unref (caps);
		gst_object_unref (videopad);
		return;
	}
	gst_caps_unref (caps);

	/* Enlaza el decodebin con clockoverlay  */
	gst_pad_link (pad, videopad);
	g_object_unref (videopad);
}

gint main (gint argc, gchar *argv[]){

	GMainLoop *loop;
	GstElement *src, *dec, *conv1, *conv2, *sink, *reloj, *filtro_sepia;
	GstPad *reloj_pad;
	GstBus *bus;
	
	/* inicia libreria GStreamer */
	gst_init (&argc, &argv);
	loop = g_main_loop_new (NULL, FALSE);

	/* Comprueba argumentos */
	if (argc != 2) {
		g_print ("Uso: %s <fichero de video>\n", argv[0]);
		return -1;
	}

	/* creacion del pipeline */
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


	/* -----------pipeline---------------------------------------------------------------------------------------*/
	/*-------------------------(ghost_pad_sink)videobin----------------------------------------------------------*/
	/* filesrc -> decodebin -> (pad)clockoverlay-->videoconvert-->coloreffects-->videoconvert-->ximagesink       */


	/* crea la parte de video: relog-->conv1-->filtro_sepia-->conv2-->sink */
	reloj = gst_element_factory_make ("clockoverlay", "hora_actual");
	filtro_sepia = gst_element_factory_make ("coloreffects", "filtro-sepia");
	video = gst_bin_new ("videobin");
	conv1 = gst_element_factory_make ("videoconvert", "conv1");
	conv2 = gst_element_factory_make ("videoconvert", "conv2");
	reloj_pad = gst_element_get_static_pad (reloj, "video_sink");
	sink = gst_element_factory_make ("ximagesink", "sink");

	if (!reloj || !filtro_sepia || !conv1 || !conv2 || !sink || !video || !reloj_pad) {
		g_printerr ("No se ha podido crear algun elemento. Saliendo.\n");
		return -1;
	}

	/* Cambiamos propiedad para que aparezca grande la hora actual */
	g_object_set (G_OBJECT(reloj), "auto-resize", FALSE, NULL);
	/* filtro sepia=2 */
	g_object_set (G_OBJECT(filtro_sepia), "preset", 2, NULL);


	gst_bin_add_many (GST_BIN (video), reloj, conv1, filtro_sepia, conv2, sink, NULL);
	gst_element_link_many (reloj, conv2, filtro_sepia, conv1, sink, NULL);

	/* Crea un ghostpad sobre el pad "video_sink" del elemento reloj */
	gst_element_add_pad (video, gst_ghost_pad_new ("ghost_pad_sink", reloj_pad));

	gst_object_unref (reloj_pad);
	
	/* Monta la tuberia completa */
	gst_bin_add (GST_BIN (pipeline), video);

	/* Estado PLAYING */
	gst_element_set_state (pipeline, GST_STATE_PLAYING);
	g_main_loop_run (loop);

	/* Salida ordenada */
	gst_element_set_state (pipeline, GST_STATE_NULL);
	gst_object_unref (GST_OBJECT (pipeline));

return 0;
}
