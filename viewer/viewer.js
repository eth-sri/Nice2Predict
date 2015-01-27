

var sgraph;
var sgraphDragListener;

function deleteGraph() {
  if (sgraph) {
    sgraph.kill();
    sgraph = undefined;
    sgraphDragListener = undefined;
  }
}

function createGraph(g) {
  deleteGraph();
  sigma.renderers.def = sigma.renderers.canvas;
  sgraph = new sigma({
    graph: createGraphRepresentation(g),
    renderer: {
      container: document.getElementById('graph-container'),
      type: 'canvas'
    },
    settings: {
      edgeLabelSize: 'proportional',      
      defaultEdgeLabelColor: "#333"
    }
  });
  relayoutGraph();
}

function relayoutGraph() {
  sgraph.startForceAtlas2({worker: true, barnesHutOptimize: false, outboundAttractionDistribution:true, iterationsPerRender:3});
  var sgr = sgraph;
  setTimeout(function() {
    if (sgraph == sgr) {
      sgraph.killForceAtlas2();
    }
  }, 1000);
  sgraphDragListener = sigma.plugins.dragNodes(sgraph, sgraph.renderers[0]);  
}

function updateGraph(g) {
  var repr = sgraph.graph;
  for (i in g.nodes) {
    var node = g.nodes[i];
    repr.nodes()[i].label = node.label;
  }
  for (i in g.edges) {
    var edge = g.edges[i];
    repr.edges()[i].label = edge.label;
  }
  sgraph.refresh();
}

function createGraphRepresentation(g) {
  console.log(g);
  var repr = { nodes:[], edges:[] };
  var N = g.nodes.length;
  for (i in g.nodes) {
    var node = g.nodes[i];
    var newnode = {
        id: node.id,
        label: node.label,
        x: 100 * Math.cos(2 * i * Math.PI / N),
        y: 100 * Math.sin(2 * i * Math.PI / N),
        size: 2.0,
        color: node.color
      };
    repr.nodes[i] = newnode;
  }
  
  for (i in g.edges) {
    var edge = g.edges[i];
    var newedge = {
        id: edge.id,
        source: edge.source,
        target: edge.target,
        label: edge.label
      };
    repr.edges[i] = newedge;
  }
  
  return repr;
}

var json_rpc_id = 0;

function callServer(server, methodName, params, success_cb, error_cb) {
  var req = {
    jsonrpc : '2.0',
    method : methodName,
    id : (++json_rpc_id)
  };
  req.params = params;

  $.ajax({
    url : server,
    data : JSON.stringify(req),
    type : "POST",
    contentType : "application/json",
    error : error_cb
  }).done(function(result) {
    success_cb(result.result);
  });
}

