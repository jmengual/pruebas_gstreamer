/*	Ejercicio4.c
		Convierte un video en formato .yuv/i420 y lo transcodifica a mp4 con codec h264		
	- Cambiar la ruta 

$ gst-launch-1.0 filesrc location=test_01.yuv ! videoparse format=i420 width=176 height=144 ! x264enc byte-stream=true ! 
video/x-h264,framerate=25/1,width=176,height=144,stream-format=byte-stream,alignment=au ! filesink location=test8.mp4

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

int
main (int   argc,
      char *argv[])
{
  GMainLoop *loop;
  GstElement *pipeline, *source, *videoParse, *encX264, *sink, *capsfilter;
  GstBus *bus;
	GstCaps* caps;


   //Initialisation
  gst_init (&argc, &argv);
  loop = g_main_loop_new (NULL, FALSE);

	/* Comprobamos argumentos */
	if (argc != 2) {
		g_printerr ("Usage: %s <YUV filename>\n", argv[0]);
		return -1;
	}

	caps = gst_caps_from_string( "video/x-h264,framerate=25/1,width=176,height=144,stream-format=byte-stream,alignment=au");
   
	/* Se crean los elementos gstreamer */
	pipeline = gst_pipeline_new ("test_pipeline");
	source = gst_element_factory_make ("filesrc", "file-source");
	videoParse = gst_element_factory_make ("videoparse", "videoparse1");
	encX264 = gst_element_factory_make ("x264enc", "codecX264");
	capsfilter = gst_element_factory_make ("capsfilter", "capsfilter");
	sink = gst_element_factory_make ("filesink", "escribeFichero");
	if (!pipeline || !source || !capsfilter || !videoParse || !encX264 || !sink) {
		g_printerr ("One element could not be created. Exiting.\n");
		return -1;
	}

	/* we set the input filename to the source element */
	g_object_set (G_OBJECT (source), "location", argv[1], NULL);
	g_object_set (G_OBJECT (videoParse), "format", 2, "width", 176, "height", 144, NULL);
	g_object_set (G_OBJECT(encX264), "byte-stream", TRUE, NULL);
	g_object_set (G_OBJECT(sink), "location","./salida2.mp4", NULL);
	g_object_set( G_OBJECT(capsfilter),  "caps",  caps, NULL );

	
  //Set up the pipeline
  //we set the input filename to the source element
  //we add a message handler
  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
  gst_bus_add_watch (bus, bus_call, loop);
  gst_object_unref (bus);
 
	/* anyadimos todos los elementos al pipeline*/
	gst_bin_add_many (GST_BIN (pipeline), source, videoParse, encX264, capsfilter,sink, NULL);

	/* we link the elements together */
	/* file-source -> -> -> conv1 ->  */
	gst_element_link_many(source, videoParse, encX264, capsfilter, sink, NULL);

   //Set the pipeline to "playing" state
  gst_element_set_state (pipeline, GST_STATE_PLAYING);
   //Iterate
  g_main_loop_run (loop);
  // Out of the main loop, clean up nicely
  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_object_unref (GST_OBJECT (pipeline));
  return 0;
}	


