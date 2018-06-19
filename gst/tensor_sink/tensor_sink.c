/*
 * GStreamer
 * Copyright (C) 2005 Thomas Vander Stichele <thomas@apestaart.org>
 * Copyright (C) 2005 Ronald S. Bultje <rbultje@ronald.bitfreak.net>
 * Copyright (C) 2018 nnstreamer <nnstreamer sec>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Alternatively, the contents of this file may be used under the
 * GNU Lesser General Public License Version 2.1 (the "LGPL"), in
 * which case the following provisions apply instead of the ones
 * mentioned above:
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * @file	tensor_sink.c
 * @date	15 June 2018
 * @brief	GStreamer plugin to handle tensor stream
 * @see		http://github.com/TO-BE-DETERMINED-SOON
 * @see		https://github.sec.samsung.net/STAR/nnstreamer
 * @author	Jaeyun Jung <jy1210.jung@samsung.com>
 *
 */

/**
 * SECTION:element-tensor_sink
 *
 * Sink element for tensor stream.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "tensor_sink.h"

/* default lateness 30ms */
#define NNS_DEFAULT_LATENESS (30 * GST_MSECOND)

GST_DEBUG_CATEGORY_STATIC (gst_tensor_sink_debug);
#define GST_CAT_DEFAULT gst_tensor_sink_debug

/* TensorSink signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_SILENT
};

static GstStaticPadTemplate sinktemplate = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

#define _do_init \
  GST_DEBUG_CATEGORY_INIT (gst_tensor_sink_debug, "tensor_sink", 0, "tensor_sink element");
#define gst_tensor_sink_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstTensorSink, gst_tensor_sink, GST_TYPE_BASE_SINK,
    _do_init);

static void gst_tensor_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_tensor_sink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_tensor_sink_start (GstBaseSink * sink);
static gboolean gst_tensor_sink_stop (GstBaseSink * sink);
static gboolean gst_tensor_sink_event (GstBaseSink * sink, GstEvent * event);
static gboolean gst_tensor_sink_query (GstBaseSink * sink, GstQuery * query);
static GstFlowReturn gst_tensor_sink_render (GstBaseSink * sink,
    GstBuffer * buffer);
static GstFlowReturn gst_tensor_sink_render_list (GstBaseSink * sink,
    GstBufferList * buffer_list);

static void _tensor_sink_render_buffer (GstTensorSink * tensor_sink,
    GstBuffer * buffer);

/* initialize tensor_sink's class */
static void
gst_tensor_sink_class_init (GstTensorSinkClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *element_class;
  GstBaseSinkClass *bsink_class;

  gobject_class = G_OBJECT_CLASS (klass);
  element_class = GST_ELEMENT_CLASS (klass);
  bsink_class = GST_BASE_SINK_CLASS (klass);

  /* gobject methods */
  gobject_class->set_property = gst_tensor_sink_set_property;
  gobject_class->get_property = gst_tensor_sink_get_property;

  /* properties */
  /* TODO: add necessary properties */
  g_object_class_install_property (gobject_class, PROP_SILENT,
      g_param_spec_boolean ("silent", "Silent", "Produce verbose output",
          FALSE, G_PARAM_READWRITE));

  gst_element_class_set_static_metadata (element_class,
      "Tensor_Sink",
      "Sink/Tensor",
      "Sink element to handle tensor stream", "nnstreamer <nnstreamer sec>");

  gst_element_class_add_static_pad_template (element_class, &sinktemplate);

  /* basesink methods */
  /* TODO: add necessary methods */
  bsink_class->start = GST_DEBUG_FUNCPTR (gst_tensor_sink_start);
  bsink_class->stop = GST_DEBUG_FUNCPTR (gst_tensor_sink_stop);
  bsink_class->event = GST_DEBUG_FUNCPTR (gst_tensor_sink_event);
  bsink_class->query = GST_DEBUG_FUNCPTR (gst_tensor_sink_query);
  bsink_class->render = GST_DEBUG_FUNCPTR (gst_tensor_sink_render);
  bsink_class->render_list = GST_DEBUG_FUNCPTR (gst_tensor_sink_render_list);
}

/* initialize tensor_sink element */
static void
gst_tensor_sink_init (GstTensorSink * tensor_sink)
{
  GstBaseSink *bsink;

  bsink = GST_BASE_SINK (tensor_sink);

  /* TODO: init properties */
  tensor_sink->silent = FALSE;

  gst_base_sink_set_sync (bsink, TRUE);
  gst_base_sink_set_max_lateness (bsink, NNS_DEFAULT_LATENESS);
  gst_base_sink_set_qos_enabled (bsink, TRUE);
}

/* gobject methods */
static void
gst_tensor_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstTensorSink *tensor_sink;

  tensor_sink = GST_TENSOR_SINK (object);

  switch (prop_id) {
    case PROP_SILENT:
      tensor_sink->silent = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_tensor_sink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstTensorSink *tensor_sink;

  tensor_sink = GST_TENSOR_SINK (object);

  switch (prop_id) {
    case PROP_SILENT:
      g_value_set_boolean (value, tensor_sink->silent);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/* basesink methods */
static gboolean
gst_tensor_sink_start (GstBaseSink * sink)
{
  /* TODO: init resources */
  return TRUE;
}

static gboolean
gst_tensor_sink_stop (GstBaseSink * sink)
{
  /* TODO: free resources */
  return TRUE;
}

static gboolean
gst_tensor_sink_event (GstBaseSink * sink, GstEvent * event)
{
  GstEventType type;

  type = GST_EVENT_TYPE (event);

  /* TODO: add event handler */
  switch (type) {
    case GST_EVENT_CAPS:
      break;

    default:
      break;
  }

  return GST_BASE_SINK_CLASS (parent_class)->event (sink, event);
}

static gboolean
gst_tensor_sink_query (GstBaseSink * sink, GstQuery * query)
{
  gboolean res = FALSE;
  GstQueryType type;

  type = GST_QUERY_TYPE (query);

  /* TODO: add query handler */
  switch (type) {
    case GST_QUERY_FORMATS:
      gst_query_set_formats (query, 2, GST_FORMAT_DEFAULT, GST_FORMAT_BYTES);
      res = TRUE;
      break;

    default:
      res = GST_BASE_SINK_CLASS (parent_class)->query (sink, query);
      break;
  }

  return res;
}

static GstFlowReturn
gst_tensor_sink_render (GstBaseSink * sink, GstBuffer * buffer)
{
  GstTensorSink *tensor_sink;

  tensor_sink = GST_TENSOR_SINK (sink);
  _tensor_sink_render_buffer (tensor_sink, buffer);

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_tensor_sink_render_list (GstBaseSink * sink, GstBufferList * buffer_list)
{
  GstTensorSink *tensor_sink;
  GstBuffer *buffer;
  guint i;
  guint num_buffers;

  tensor_sink = GST_TENSOR_SINK (sink);
  num_buffers = gst_buffer_list_length (buffer_list);

  for (i = 0; i < num_buffers; i++) {
    buffer = gst_buffer_list_get (buffer_list, i);
    _tensor_sink_render_buffer (tensor_sink, buffer);
  }

  return GST_FLOW_OK;
}

static void
_tensor_sink_render_buffer (GstTensorSink * tensor_sink, GstBuffer * buffer)
{
  GstMemory *mem;
  GstMapInfo info;
  guint i;
  guint num_mems;

  num_mems = gst_buffer_n_memory (buffer);

  for (i = 0; i < num_mems; i++) {
    mem = gst_buffer_peek_memory (buffer, i);

    if (gst_memory_map (mem, &info, GST_MAP_READ)) {
      /* TODO: handle buffers (info.data, info.size) */

      gst_memory_unmap (mem, &info);
    }
  }
}
