
#include "stdafx.hpp"

#include <string>
#include <string_view>

#include "md_transpile.hpp"

#include "cmark.h"
#include "node.h"

static void node_walker(md::url_replacer replacer, cmark_node* node, cmark_event_type ev_type)
{
	auto cmark_mem = node->mem;

	if (node->type == CMARK_NODE_IMAGE && ev_type == CMARK_EVENT_EXIT)
	{
		unsigned char* old_url = node->as.link.url;
		auto new_url = replacer(std::string((char*)old_url));
		node->as.link.url = reinterpret_cast<unsigned char*>(cmark_mem->calloc(new_url.length() + 1, 1));
		std::memcpy(node->as.link.url, new_url.c_str(), new_url.length() + 1);
		cmark_mem->free(old_url);
	}
}

std::string md::markdown_transpile(std::string_view original, url_replacer replacer)
{
	std::shared_ptr<cmark_node> ast;

	ast.reset(
        cmark_parse_document(original.data(), original.length(), CMARK_OPT_VALIDATE_UTF8 | CMARK_OPT_UNSAFE | CMARK_OPT_SMART),
		cmark_node_free
    );
//    auto root_node = ast.get();

	auto cmark_mem = ast->mem;

	cmark_event_type ev_type;
	cmark_node* cur;
	std::shared_ptr<cmark_iter> iter;
	iter.reset(cmark_iter_new(ast.get()), cmark_iter_free);

	while ((ev_type = cmark_iter_next(iter.get())) != CMARK_EVENT_DONE)
	{
		cur = cmark_iter_get_node(iter.get());
		node_walker(replacer, cur, ev_type);
	}

	auto html = cmark_render_commonmark(ast.get(), CMARK_OPT_VALIDATE_UTF8 | CMARK_OPT_UNSAFE | CMARK_OPT_SMART, 140);

	std::string ret(html);
	cmark_mem->free(html);

	return ret;
}

