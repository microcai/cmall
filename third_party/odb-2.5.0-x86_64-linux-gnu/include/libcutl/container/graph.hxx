// file      : libcutl/container/graph.hxx
// license   : MIT; see accompanying LICENSE file

#ifndef LIBCUTL_CONTAINER_GRAPH_HXX
#define LIBCUTL_CONTAINER_GRAPH_HXX

#include <map>

#include <libcutl/exception.hxx>
#include <libcutl/shared-ptr.hxx>

namespace cutl
{
  namespace container
  {
    struct no_edge: exception {};
    struct no_node: exception {};

    template <typename N, typename E>
    class graph
    {
    public:
      typedef N node_base;
      typedef E edge_base;

    public:
      template <typename T>
      T&
      new_node ();

      template <typename T, typename A0>
      T&
      new_node (A0 const&);

      template <typename T, typename A0, typename A1>
      T&
      new_node (A0 const&, A1 const&);

      template <typename T, typename A0, typename A1, typename A2>
      T&
      new_node (A0 const&, A1 const&, A2 const&);

      template <typename T, typename A0, typename A1, typename A2,
                typename A3>
      T&
      new_node (A0 const&, A1 const&, A2 const&, A3 const&);

      template <typename T, typename A0, typename A1, typename A2,
                typename A3, typename A4>
      T&
      new_node (A0 const&, A1 const&, A2 const&, A3 const&, A4 const&);

      template <typename T, typename A0, typename A1, typename A2,
                typename A3, typename A4, typename A5>
      T&
      new_node (A0 const&, A1 const&, A2 const&, A3 const&, A4 const&,
                A5 const&);

      template <typename T, typename A0, typename A1, typename A2,
                typename A3, typename A4, typename A5, typename A6>
      T&
      new_node (A0 const&, A1 const&, A2 const&, A3 const&, A4 const&,
                A5 const&, A6 const&);

      template <typename T, typename A0, typename A1, typename A2,
                typename A3, typename A4, typename A5, typename A6,
                typename A7>
      T&
      new_node (A0 const&, A1 const&, A2 const&, A3 const&, A4 const&,
                A5 const&, A6 const&, A7 const&);

      template <typename T, typename A0, typename A1, typename A2,
                typename A3, typename A4, typename A5, typename A6,
                typename A7, typename A8>
      T&
      new_node (A0 const&, A1 const&, A2 const&, A3 const&, A4 const&,
                A5 const&, A6 const&, A7 const&, A8 const&);

      template <typename T, typename A0, typename A1, typename A2,
                typename A3, typename A4, typename A5, typename A6,
                typename A7, typename A8, typename A9>
      T&
      new_node (A0 const&, A1 const&, A2 const&, A3 const&, A4 const&,
                A5 const&, A6 const&, A7 const&, A8 const&, A9 const&);

      // Non-const versions.
      //
      template <typename T, typename A0>
      T&
      new_node (A0&);

      template <typename T, typename A0, typename A1>
      T&
      new_node (A0&, A1&);

      template <typename T, typename A0, typename A1, typename A2>
      T&
      new_node (A0&, A1&, A2&);

    public:
      template <typename T, typename L, typename R>
      T&
      new_edge (L&, R&);

      template <typename T, typename L, typename R,
                typename A0>
      T&
      new_edge (L&, R&, A0 const&);

      template <typename T, typename L, typename R,
                typename A0, typename A1>
      T&
      new_edge (L&, R&, A0 const&, A1 const&);

      template <typename T, typename L, typename R,
                typename A0, typename A1, typename A2>
      T&
      new_edge (L&, R&, A0 const&, A1 const&, A2 const&);

      template <typename T, typename L, typename R,
                typename A0, typename A1, typename A2, typename A3>
      T&
      new_edge (L&, R&, A0 const&, A1 const&, A2 const&, A3 const&);

      template <typename T, typename L, typename R,
                typename A0, typename A1, typename A2, typename A3,
                typename A4>
      T&
      new_edge (L&, R&, A0 const&, A1 const&, A2 const&, A3 const&,
                A4 const&);

      template <typename T, typename L, typename R,
                typename A0, typename A1, typename A2, typename A3,
                typename A4, typename A5>
      T&
      new_edge (L&, R&, A0 const&, A1 const&, A2 const&, A3 const&,
                A4 const&, A5 const&);

      // Functions to reset edge's nodes.
      //
    public:
      template <typename TE, typename TN>
      void
      reset_left_node (TE& edge, TN& node)
      {
        edge.set_left_node (node);
      }

      template <typename TE, typename TN>
      void
      reset_right_node (TE& edge, TN& node)
      {
        edge.set_right_node (node);
      }

      // Functions to add edges to a node.
      //
    public:
      template <typename TN, typename TE>
      void
      add_edge_left (TN& node, TE& edge)
      {
        node.add_edge_left (edge);
      }

      template <typename TN, typename TE>
      void
      add_edge_right (TN& node, TE& edge)
      {
        node.add_edge_right (edge);
      }

      // Functions to delete edges and nodes. In order to delete a
      // a node without leaving any dangling edges you need to make
      // sure that each edge pointing to it is either deleted or reset
      // to some other node.
      //
    public:
      template <typename T, typename L, typename R>
      void
      delete_edge (L& left_node, R& right_node, T& edge);

      void
      delete_node (node_base& node)
      {
        if (nodes_.erase (&node) == 0)
          throw no_node ();
      }

    public:
      graph () {}

    private:
      graph (graph const&);

      graph&
      operator= (graph const&);

    protected:
      typedef shared_ptr<node_base> node_ptr;
      typedef shared_ptr<edge_base> edge_ptr;

      typedef std::map<node_base*, node_ptr> nodes;
      typedef std::map<edge_base*, edge_ptr> edges;

      nodes nodes_;
      edges edges_;
    };
  }
}

#include <libcutl/container/graph.txx>

#endif  // CUTL_CONTAINER_GRAPH_HXX
