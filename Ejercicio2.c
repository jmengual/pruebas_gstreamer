/*
	Ejercicio2: Muestra la hora actual sobre el video en formato ogv
*/

#include <gst/gst.h>
#include <glib.h>


static gboolean bus_call 
					(GstBus     *bus,
          GstMessage *msg,
          gpointer    data)

{
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

static void on_pad_added (GstElement *element, GstPad *pad, gpointer data){

	GstPad *sinkpad;
	GstElement *decoder = (GstElement *) data;

	/* We can now link this pad with the vorbis-decoder sink pad */
	g_print ("Dynamic pad created, linking demuxer/decoder\n");
	sinkpad = gst_element_get_static_pad (decoder, "sink"); // Solicita un sink-pad al decodificador
	gst_pad_link (pad, sinkpad);
	gst_object_unref (sinkpad);

}

int
main (int   argc,
      char *argv[])
{
  GMainLoop *loop;
  GstElement *pipeline, *source, *demuxer, *decoder, *conv, *sink;
  GstElement * hora;
  GstBus *bus;

   //Initialisation
  gst_init (&argc, &argv);
  loop = g_main_loop_new (NULL, FALSE);

	/* Comprobamos argumentos */
	if (argc != 2) {
		g_printerr ("Usage: %s <Ogg/Vorbis filename>\n", argv[0]);
		return -1;
	}
   
	/* Se crean los elementos gstreamer */
	pipeline = gst_pipeline_new ("test_pipeline");
	source = gst_element_factory_make ("filesrc", "file-source");
	demuxer = gst_element_factory_make ("oggdemux", "ogg-demuxer");
	decoder = gst_element_factory_make ("theoradec", "theoradec-decoder");
  hora = gst_element_factory_make ("clockoverlay", "hora_actual");
	conv = gst_element_factory_make ("videoconvert", "converter");
	sink = gst_element_factory_make ("ximagesink", "video_out");

	if (!pipeline || !source || !demuxer || !decoder || !hora || !conv || !sink) {
		g_printerr ("One element could not be created. Exiting.\n");
		return -1;
	}

	/* we set the input filename to the source element */
	g_object_set (G_OBJECT (source), "location", argv[1], NULL);
	
	// Cambiamos propiedad para que aparezca grande la hora actual
	g_object_set (G_OBJECT(hora), "auto-resize", FALSE, NULL);

  //Set up the pipeline
  //we set the input filename to the source element
  //we add a message handler
  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
  gst_bus_add_watch (bus, bus_call, loop);
  gst_object_unref (bus);
 
	/* anyadimos todos los elementos al pipeline*/
	/* file-source | ogg-demuxer | theoradec-decoder | hora_actual | converter | video_out */
	gst_bin_add_many (GST_BIN (pipeline), source, demuxer, decoder, hora, conv, sink, NULL);

	/* we link the elements together */
	/* file-source -> demuxer -> vorbis-decoder -> hora -> converter -> sink */
	gst_element_link (source, demuxer);
	gst_element_link_many (decoder, hora, conv, sink, NULL);

	g_signal_connect (demuxer, "pad-added", G_CALLBACK (on_pad_added), decoder);

   //Set the pipeline to "playing" state
  gst_element_set_state (pipeline, GST_STATE_PLAYING);
   //Iterate
  g_main_loop_run (loop);
  // Out of the main loop, clean up nicely
  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_object_unref (GST_OBJECT (pipeline));
  return 0;
}	


