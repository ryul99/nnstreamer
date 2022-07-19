/* SPDX-License-Identifier: LGPL-2.1-only */
/**
 * Copyright (C) 2021 Samsung Electronics Co., Ltd.
 *
 * @file    tensor_query_serversrc.c
 * @date    09 Jul 2021
 * @brief   GStreamer plugin to handle tensor query_server src
 * @author  Junhwan Kim <jejudo.kim@samsung.com>
 * @see     http://github.com/nnstreamer/nnstreamer
 * @bug     No known bugs
 *
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <tensor_typedef.h>
#include <tensor_common.h>
#include "tensor_query_serversrc.h"
#include "tensor_query_server.h"
#include "tensor_query_common.h"
#include "nnstreamer_util.h"

GST_DEBUG_CATEGORY_STATIC (gst_tensor_query_serversrc_debug);
#define GST_CAT_DEFAULT gst_tensor_query_serversrc_debug

#define DEFAULT_PORT_SRC 3001
#define DEFAULT_IS_LIVE TRUE
/**
 * @brief the capabilities of the outputs
 */
static GstStaticPadTemplate srctemplate = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

/**
 * @brief query_serversrc properties
 */
enum
{
  PROP_0,
  PROP_HOST,
  PROP_PORT,
  PROP_PROTOCOL,
  PROP_TIMEOUT,
  PROP_OPERATION,
  PROP_ID,
  PROP_IS_LIVE
};

#define gst_tensor_query_serversrc_parent_class parent_class
G_DEFINE_TYPE (GstTensorQueryServerSrc, gst_tensor_query_serversrc,
    GST_TYPE_PUSH_SRC);

static void gst_tensor_query_serversrc_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_tensor_query_serversrc_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);
static void gst_tensor_query_serversrc_finalize (GObject * object);

static gboolean gst_tensor_query_serversrc_start (GstBaseSrc * bsrc);
static GstFlowReturn gst_tensor_query_serversrc_create (GstPushSrc * psrc,
    GstBuffer ** buf);

/**
 * @brief initialize the query_serversrc class
 */
static void
gst_tensor_query_serversrc_class_init (GstTensorQueryServerSrcClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstBaseSrcClass *gstbasesrc_class;
  GstPushSrcClass *gstpushsrc_class;

  gstpushsrc_class = (GstPushSrcClass *) klass;
  gstbasesrc_class = (GstBaseSrcClass *) gstpushsrc_class;
  gstelement_class = (GstElementClass *) gstbasesrc_class;
  gobject_class = (GObjectClass *) gstelement_class;

  gobject_class->set_property = gst_tensor_query_serversrc_set_property;
  gobject_class->get_property = gst_tensor_query_serversrc_get_property;
  gobject_class->finalize = gst_tensor_query_serversrc_finalize;

  g_object_class_install_property (gobject_class, PROP_HOST,
      g_param_spec_string ("host", "Host", "The hostname to listen as",
          DEFAULT_HOST, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_PORT,
      g_param_spec_uint ("port", "Port",
          "The port to listen to (0=random available port)", 0,
          65535, DEFAULT_PORT_SRC, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_PROTOCOL,
      g_param_spec_enum ("protocol", "Protocol",
          "The network protocol to establish connections between client and server.",
          GST_TYPE_QUERY_PROTOCOL, DEFAULT_PROTOCOL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_TIMEOUT,
      g_param_spec_uint ("timeout", "Timeout",
          "The timeout as seconds to maintain connection", 0,
          3600, QUERY_DEFAULT_TIMEOUT_SEC,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_OPERATION,
      g_param_spec_string ("topic", "Topic",
          "The main topic of the host and option if necessary. "
          "(topic)/(optional topic for main topic).",
          "", G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_ID,
      g_param_spec_uint ("id", "ID",
          "ID for distinguishing query servers.", 0,
          G_MAXUINT, DEFAULT_SERVER_ID,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_IS_LIVE,
      g_param_spec_boolean ("is-live", "Is Live",
          "Synchronize the incoming buffers' timestamp with the current running time",
          DEFAULT_IS_LIVE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&srctemplate));

  gst_element_class_set_static_metadata (gstelement_class,
      "TensorQueryServerSrc", "Source/Tensor/Query",
      "Receive tensor data as a server over the network",
      "Samsung Electronics Co., Ltd.");

  gstbasesrc_class->start = gst_tensor_query_serversrc_start;
  gstpushsrc_class->create = gst_tensor_query_serversrc_create;

  GST_DEBUG_CATEGORY_INIT (gst_tensor_query_serversrc_debug,
      "tensor_query_serversrc", 0, "Tensor Query Server Source");
}

/**
 * @brief initialize the new query_serversrc element
 */
static void
gst_tensor_query_serversrc_init (GstTensorQueryServerSrc * src)
{
  src->host = g_strdup (DEFAULT_HOST);
  src->port = DEFAULT_PORT_SRC;
  src->protocol = DEFAULT_PROTOCOL;
  src->timeout = QUERY_DEFAULT_TIMEOUT_SEC;
  src->topic = NULL;
  src->srv_host = g_strdup (DEFAULT_HOST);
  src->srv_port = DEFAULT_PORT_SRC;
  src->src_id = DEFAULT_SERVER_ID;
  tensor_query_hybrid_init (&src->hybrid_info, NULL, 0, TRUE);
  src->configured = FALSE;
  src->msg_queue = g_async_queue_new ();

  gst_base_src_set_format (GST_BASE_SRC (src), GST_FORMAT_TIME);
  /** set the timestamps on each buffer */
  gst_base_src_set_do_timestamp (GST_BASE_SRC (src), TRUE);
  /** set the source to be live */
  gst_base_src_set_live (GST_BASE_SRC (src), DEFAULT_IS_LIVE);
}

/**
 * @brief finalize the query_serversrc object
 */
static void
gst_tensor_query_serversrc_finalize (GObject * object)
{
  GstTensorQueryServerSrc *src = GST_TENSOR_QUERY_SERVERSRC (object);
  nns_edge_data_h data_h;

  g_free (src->host);
  src->host = NULL;
  g_free (src->topic);
  src->topic = NULL;
  g_free (src->srv_host);
  src->srv_host = NULL;

  while ((data_h = g_async_queue_try_pop (src->msg_queue))) {
    nns_edge_data_destroy (data_h);
  }
  g_async_queue_unref (src->msg_queue);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

/**
 * @brief set property of query_serversrc
 */
static void
gst_tensor_query_serversrc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstTensorQueryServerSrc *serversrc = GST_TENSOR_QUERY_SERVERSRC (object);

  switch (prop_id) {
    case PROP_HOST:
      if (!g_value_get_string (value)) {
        nns_logw ("host property cannot be NULL");
        break;
      }
      g_free (serversrc->host);
      serversrc->host = g_value_dup_string (value);
      break;
    case PROP_PORT:
      serversrc->port = g_value_get_uint (value);
      break;
    case PROP_PROTOCOL:
      serversrc->protocol = g_value_get_enum (value);
      break;
    case PROP_TIMEOUT:
      serversrc->timeout = g_value_get_uint (value);
      break;
    case PROP_OPERATION:
      if (!g_value_get_string (value)) {
        nns_logw ("topic property cannot be NULL. Query-hybrid is disabled.");
        break;
      }
      g_free (serversrc->topic);
      serversrc->topic = g_value_dup_string (value);
      break;
    case PROP_ID:
      serversrc->src_id = g_value_get_uint (value);
      break;
    case PROP_IS_LIVE:
      gst_base_src_set_live (GST_BASE_SRC (serversrc),
          g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/**
 * @brief get property of query_serversrc
 */
static void
gst_tensor_query_serversrc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstTensorQueryServerSrc *serversrc = GST_TENSOR_QUERY_SERVERSRC (object);

  switch (prop_id) {
    case PROP_HOST:
      g_value_set_string (value, serversrc->host);
      break;
    case PROP_PORT:
      g_value_set_uint (value, serversrc->port);
      break;
    case PROP_PROTOCOL:
      g_value_set_enum (value, serversrc->protocol);
      break;
    case PROP_TIMEOUT:
      g_value_set_uint (value, serversrc->timeout);
      break;
    case PROP_OPERATION:
      g_value_set_string (value, serversrc->topic);
      break;
    case PROP_ID:
      g_value_set_uint (value, serversrc->src_id);
      break;
    case PROP_IS_LIVE:
      g_value_set_boolean (value,
          gst_base_src_is_live (GST_BASE_SRC (serversrc)));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/**
 * @brief nnstreamer-edge event callback.
 */
static int
_nns_edge_event_cb (nns_edge_event_h event_h, void *user_data)
{
  nns_edge_event_e event_type;
  int ret = NNS_EDGE_ERROR_NONE;

  GstTensorQueryServerSrc *src = (GstTensorQueryServerSrc *) user_data;
  if (0 != nns_edge_event_get_type (event_h, &event_type)) {
    nns_loge ("Failed to get event type!");
    return NNS_EDGE_ERROR_UNKNOWN;
  }

  switch (event_type) {
    case NNS_EDGE_EVENT_NEW_DATA_RECEIVED:
    {
      nns_edge_data_h data;

      nns_edge_event_parse_new_data (event_h, &data);
      g_async_queue_push (src->msg_queue, data);
      break;
    }
    default:
      break;
  }

  return ret;
}

/**
 * @brief start processing of query_serversrc, setting up the server
 */
static gboolean
gst_tensor_query_serversrc_start (GstBaseSrc * bsrc)
{
  GstTensorQueryServerSrc *src = GST_TENSOR_QUERY_SERVERSRC (bsrc);
  char *id_str = NULL;
  char *port = NULL;

  id_str = g_strdup_printf ("%d", src->src_id);
  src->server_h = gst_tensor_query_server_add_data (id_str);
  g_free (id_str);

  src->edge_h = gst_tensor_query_server_get_edge_handle (src->server_h);
  nns_edge_set_info (src->edge_h, "IP", src->host);
  port = g_strdup_printf ("%d", src->port);
  nns_edge_set_info (src->edge_h, "PORT", port);
  g_free (port);

  /** Publish query sever connection info */
  if (src->topic) {
    nns_edge_set_info (src->edge_h, "TOPIC", src->topic);
    tensor_query_hybrid_set_node (&src->hybrid_info, src->srv_host,
        src->srv_port, src->server_info_h);
    tensor_query_hybrid_set_broker (&src->hybrid_info, src->host, src->port);

    if (!tensor_query_hybrid_publish (&src->hybrid_info, src->topic)) {
      nns_loge ("Failed to publish a topic.");
      return FALSE;
    }
  } else {
    g_free (src->srv_host);
    src->srv_host = g_strdup (src->host);
    src->srv_port = src->port;
    nns_logi ("Query-hybrid feature is disabled.");
    nns_logi
        ("Specify topic to register server to broker (e.g., topic=object_detection/mobilev3).");
  }
  nns_edge_set_event_callback (src->edge_h, _nns_edge_event_cb, src);

  if (0 != nns_edge_start (src->edge_h, true)) {
    nns_loge
        ("Failed to start NNStreamer-edge. Please check server IP and port");
    return FALSE;
  }

  if (!gst_tensor_query_server_wait_sink (src->server_h)) {
    nns_loge ("Failed to get server information from query server.");
    return FALSE;
  }

  return TRUE;
}

/**
 * @brief Get buffer from message queue.
 */
static GstBuffer *
_gst_tensor_query_serversrc_get_buffer (GstTensorQueryServerSrc * src)
{
  nns_edge_data_h data_h;
  GstBuffer *buffer = NULL;
  guint i, num_data;
  GstMetaQuery *meta_query;

  data_h = g_async_queue_pop (src->msg_queue);

  if (!data_h) {
    nns_loge ("Failed to get message from the server message queue");
    return NULL;
  }
  buffer = gst_buffer_new ();

  if (NNS_EDGE_ERROR_NONE != nns_edge_data_get_count (data_h, &num_data)) {
    nns_loge ("Failed to get the number of memories of the edge data.");
    gst_buffer_unref (buffer);
    return NULL;
  }
  for (i = 0; i < num_data; i++) {
    void *data = NULL;
    size_t data_len = 0;
    gpointer new_data;

    nns_edge_data_get (data_h, i, &data, &data_len);
    new_data = _g_memdup (data, data_len);

    gst_buffer_append_memory (buffer,
        gst_memory_new_wrapped (0, new_data, data_len, 0, data_len, new_data,
            g_free));
  }

  if (buffer) {
    meta_query = gst_buffer_add_meta_query (buffer);
    if (meta_query) {
      char *val;
      int ret;

      ret = nns_edge_data_get_info (data_h, "client_id", &val);
      if (NNS_EDGE_ERROR_NONE != ret) {
        gst_buffer_unref (buffer);
        buffer = NULL;
      } else {
        meta_query->client_id = g_ascii_strtoll (val, NULL, 10);
        g_free (val);
      }
    }
  }
  nns_edge_data_destroy (data_h);

  return buffer;
}

/**
 * @brief create query_serversrc, wait on socket and receive data
 */
static GstFlowReturn
gst_tensor_query_serversrc_create (GstPushSrc * psrc, GstBuffer ** outbuf)
{
  GstTensorQueryServerSrc *src = GST_TENSOR_QUERY_SERVERSRC (psrc);
  GstBaseSrc *bsrc = GST_BASE_SRC (psrc);

  if (!src->configured) {
    gchar *caps_str, *prev_caps_str, *new_caps_str;

    GstCaps *caps = gst_pad_peer_query_caps (GST_BASE_SRC_PAD (bsrc), NULL);
    if (gst_caps_is_fixed (caps)) {
      gst_base_src_set_caps (bsrc, caps);
    }

    caps_str = gst_caps_to_string (caps);

    nns_edge_get_info (src->edge_h, "CAPS", &prev_caps_str);
    new_caps_str = g_strdup_printf ("%s@query_server_src_caps@%s",
        prev_caps_str, caps_str);
    nns_edge_set_info (src->edge_h, "CAPS", new_caps_str);
    g_free (prev_caps_str);
    g_free (new_caps_str);
    g_free (caps_str);

    gst_caps_unref (caps);
    src->configured = TRUE;
  }

  *outbuf = _gst_tensor_query_serversrc_get_buffer (src);
  if (!outbuf) {
    nns_loge ("Failed to get buffer to push to the tensor query serversrc.");
    return GST_FLOW_ERROR;
  }

  return GST_FLOW_OK;
}
