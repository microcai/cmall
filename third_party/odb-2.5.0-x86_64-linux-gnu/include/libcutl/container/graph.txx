// file      : libcutl/container/graph.txx
// license   : MIT; see accompanying LICENSE file

namespace cutl
{
  namespace container
  {

    // Nodes.
    //

    template <typename N, typename E>
    template <typename T>
    T& graph<N, E>::
    new_node ()
    {
      shared_ptr<T> node (new (shared) T);
      nodes_[node.get ()] = node;

      return *node;
    }


    template <typename N, typename E>
    template <typename T, typename A0>
    T& graph<N, E>::
    new_node (A0 const& a0)
    {
      shared_ptr<T> node (new (shared) T (a0));
      nodes_[node.get ()] = node;

      return *node;
    }


    template <typename N, typename E>
    template <typename T, typename A0, typename A1>
    T& graph<N, E>::
    new_node (A0 const& a0, A1 const& a1)
    {
      shared_ptr<T> node (new (shared) T (a0, a1));
      nodes_[node.get ()] = node;

      return *node;
    }

    template <typename N, typename E>
    template <typename T, typename A0, typename A1, typename A2>
    T& graph<N, E>::
    new_node (A0 const& a0, A1 const& a1, A2 const& a2)
    {
      shared_ptr<T> node (new (shared) T (a0, a1, a2));
      nodes_[node.get ()] = node;

      return *node;
    }

    template <typename N, typename E>
    template <typename T, typename A0, typename A1, typename A2,
              typename A3>
    T& graph<N, E>::
    new_node (A0 const& a0, A1 const& a1, A2 const& a2, A3 const& a3)
    {
      shared_ptr<T> node (new (shared) T (a0, a1, a2, a3));
      nodes_[node.get ()] = node;

      return *node;
    }

    template <typename N, typename E>
    template <typename T, typename A0, typename A1, typename A2,
              typename A3, typename A4>
    T& graph<N, E>::
    new_node (A0 const& a0, A1 const& a1, A2 const& a2, A3 const& a3,
              A4 const& a4)
    {
      shared_ptr<T> node (new (shared) T (a0, a1, a2, a3, a4));
      nodes_[node.get ()] = node;

      return *node;
    }

    template <typename N, typename E>
    template <typename T, typename A0, typename A1, typename A2,
              typename A3, typename A4, typename A5>
    T& graph<N, E>::
    new_node (A0 const& a0, A1 const& a1, A2 const& a2, A3 const& a3,
              A4 const& a4, A5 const& a5)
    {
      shared_ptr<T> node (new (shared) T (a0, a1, a2, a3, a4, a5));
      nodes_[node.get ()] = node;

      return *node;
    }

    template <typename N, typename E>
    template <typename T, typename A0, typename A1, typename A2,
              typename A3, typename A4, typename A5, typename A6>
    T& graph<N, E>::
    new_node (A0 const& a0, A1 const& a1, A2 const& a2, A3 const& a3,
              A4 const& a4, A5 const& a5, A6 const& a6)
    {
      shared_ptr<T> node (new (shared) T (a0, a1, a2, a3, a4, a5, a6));
      nodes_[node.get ()] = node;

      return *node;
    }

    template <typename N, typename E>
    template <typename T, typename A0, typename A1, typename A2,
              typename A3, typename A4, typename A5, typename A6,
              typename A7>
    T& graph<N, E>::
    new_node (A0 const& a0, A1 const& a1, A2 const& a2, A3 const& a3,
              A4 const& a4, A5 const& a5, A6 const& a6, A7 const& a7)
    {
      shared_ptr<T> node (new (shared) T (a0, a1, a2, a3, a4, a5, a6, a7));
      nodes_[node.get ()] = node;

      return *node;
    }


    template <typename N, typename E>
    template <typename T, typename A0, typename A1, typename A2,
              typename A3, typename A4, typename A5, typename A6,
              typename A7, typename A8>
    T& graph<N, E>::
    new_node (A0 const& a0, A1 const& a1, A2 const& a2, A3 const& a3,
              A4 const& a4, A5 const& a5, A6 const& a6, A7 const& a7,
              A8 const& a8)
    {
      shared_ptr<T> node (
        new (shared) T (a0, a1, a2, a3, a4, a5, a6, a7, a8));
      nodes_[node.get ()] = node;

      return *node;
    }


    template <typename N, typename E>
    template <typename T, typename A0, typename A1, typename A2,
              typename A3, typename A4, typename A5, typename A6,
              typename A7, typename A8, typename A9>
    T& graph<N, E>::
    new_node (A0 const& a0, A1 const& a1, A2 const& a2, A3 const& a3,
              A4 const& a4, A5 const& a5, A6 const& a6, A7 const& a7,
              A8 const& a8, A9 const& a9)
    {
      shared_ptr<T> node (
        new (shared) T (a0, a1, a2, a3, a4, a5, a6, a7, a8, a9));
      nodes_[node.get ()] = node;

      return *node;
    }

    // Non-const versions.
    //
    template <typename N, typename E>
    template <typename T, typename A0>
    T& graph<N, E>::
    new_node (A0& a0)
    {
      shared_ptr<T> node (new (shared) T (a0));
      nodes_[node.get ()] = node;

      return *node;
    }


    template <typename N, typename E>
    template <typename T, typename A0, typename A1>
    T& graph<N, E>::
    new_node (A0& a0, A1& a1)
    {
      shared_ptr<T> node (new (shared) T (a0, a1));
      nodes_[node.get ()] = node;

      return *node;
    }

    template <typename N, typename E>
    template <typename T, typename A0, typename A1, typename A2>
    T& graph<N, E>::
    new_node (A0& a0, A1& a1, A2& a2)
    {
      shared_ptr<T> node (new (shared) T (a0, a1, a2));
      nodes_[node.get ()] = node;

      return *node;
    }

    // Edges.
    //

    template <typename N, typename E>
    template <typename T, typename L, typename R>
    T& graph<N, E>::
    new_edge (L& l, R& r)
    {
      shared_ptr<T> edge (new (shared) T);
      edges_[edge.get ()] = edge;

      edge->set_left_node (l);
      edge->set_right_node (r);

      l.add_edge_left (*edge);
      r.add_edge_right (*edge);

      return *edge;
    }

    template <typename N, typename E>
    template <typename T, typename L, typename R,
              typename A0>
    T& graph<N, E>::
    new_edge (L& l, R& r, A0 const& a0)
    {
      shared_ptr<T> edge (new (shared) T (a0));
      edges_[edge.get ()] = edge;

      edge->set_left_node (l);
      edge->set_right_node (r);

      l.add_edge_left (*edge);
      r.add_edge_right (*edge);

      return *edge;
    }

    template <typename N, typename E>
    template <typename T, typename L, typename R,
              typename A0, typename A1>
    T& graph<N, E>::
    new_edge (L& l, R& r, A0 const& a0, A1 const& a1)
    {
      shared_ptr<T> edge (new (shared) T (a0, a1));
      edges_[edge.get ()] = edge;

      edge->set_left_node (l);
      edge->set_right_node (r);

      l.add_edge_left (*edge);
      r.add_edge_right (*edge);

      return *edge;
    }

    template <typename N, typename E>
    template <typename T, typename L, typename R,
              typename A0, typename A1, typename A2>
    T& graph<N, E>::
    new_edge (L& l, R& r, A0 const& a0, A1 const& a1, A2 const& a2)
    {
      shared_ptr<T> edge (new (shared) T (a0, a1, a2));
      edges_[edge.get ()] = edge;

      edge->set_left_node (l);
      edge->set_right_node (r);

      l.add_edge_left (*edge);
      r.add_edge_right (*edge);

      return *edge;
    }

    template <typename N, typename E>
    template <typename T, typename L, typename R,
              typename A0, typename A1, typename A2, typename A3>
    T& graph<N, E>::
    new_edge (L& l, R& r, A0 const& a0, A1 const& a1, A2 const& a2,
              A3 const& a3)
    {
      shared_ptr<T> edge (new (shared) T (a0, a1, a2, a3));
      edges_[edge.get ()] = edge;

      edge->set_left_node (l);
      edge->set_right_node (r);

      l.add_edge_left (*edge);
      r.add_edge_right (*edge);

      return *edge;
    }

    template <typename N, typename E>
    template <typename T, typename L, typename R,
              typename A0, typename A1, typename A2, typename A3,
              typename A4>
    T& graph<N, E>::
    new_edge (L& l, R& r, A0 const& a0, A1 const& a1, A2 const& a2,
              A3 const& a3, A4 const& a4)
    {
      shared_ptr<T> edge (new (shared) T (a0, a1, a2, a3, a4));
      edges_[edge.get ()] = edge;

      edge->set_left_node (l);
      edge->set_right_node (r);

      l.add_edge_left (*edge);
      r.add_edge_right (*edge);

      return *edge;
    }

    template <typename N, typename E>
    template <typename T, typename L, typename R,
              typename A0, typename A1, typename A2, typename A3,
              typename A4, typename A5>
    T& graph<N, E>::
    new_edge (L& l, R& r, A0 const& a0, A1 const& a1, A2 const& a2,
              A3 const& a3, A4 const& a4, A5 const& a5)
    {
      shared_ptr<T> edge (new (shared) T (a0, a1, a2, a3, a4, a5));
      edges_[edge.get ()] = edge;

      edge->set_left_node (l);
      edge->set_right_node (r);

      l.add_edge_left (*edge);
      r.add_edge_right (*edge);

      return *edge;
    }


    template <typename N, typename E>
    template <typename T, typename L, typename R>
    void graph<N, E>::
    delete_edge (L& l, R& r, T& edge)
    {
      typename edges::iterator i (edges_.find (&edge));

      if (i == edges_.end () ||
          nodes_.find (&l) == nodes_.end () ||
          nodes_.find (&r) == nodes_.end ())
        throw no_edge ();

      r.remove_edge_right (edge);
      l.remove_edge_left (edge);

      edge.clear_right_node (r);
      edge.clear_left_node (l);

      edges_.erase (i);
    }
  }
}
