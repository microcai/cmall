
#pragma once

#include <map>
#include <ctime>
#include <cstring>
#include <cctype>

#include "minizip/unzip.h"

struct Res {
	const char *data;
	size_t size;
};
extern "C" Res SITE(void);

namespace{


	struct exe_bundle_file
	{
		voidpf base_ptr;
		ZPOS64_T offset;
		uLong fsize;
		int error;

		exe_bundle_file(voidpf _base, uLong _size)
			: base_ptr(_base)
			, offset(0)
			, fsize(_size)
			, error(0)
		{}
	};

	zlib_filefunc64_def exe_bundled_file_ops =
	{
		// .zopen64_file =
		[](voidpf, const void* , int ) -> voidpf
		{
			std::size_t zip_size;
			voidpf zip_content;

			auto res = SITE();

			zip_content = const_cast<char*>(res.data);
			zip_size = res.size;

			// return membaseptr as filestream !
			auto filestream = new exe_bundle_file((voidpf)zip_content, (uLong)zip_size);
			return filestream;
		},
		// .zread_file =
		[](voidpf, voidpf stream, void* buf, uLong size) -> uLong
		{
			exe_bundle_file * f = reinterpret_cast<exe_bundle_file*>(stream);
			if (f->error == 0)
			{
				uLong copyable_size = std::min(size, uLong(f->fsize - f->offset));
				if (copyable_size > 0)
				{
					memcpy(buf, reinterpret_cast<char*>(f->base_ptr) + f->offset, copyable_size);
					f->offset += copyable_size;
					return copyable_size;
				}
				f->error = EOF;
				return 0;
			}
			return (uLong) -1;
		},
		// .zwrite_file =
		[](voidpf, voidpf, const void*, uLong) -> uLong
		{
			return (uLong)-1;
		},
		// .ztell64_file =
		[](voidpf, voidpf stream) -> ZPOS64_T
		{
			exe_bundle_file * f = reinterpret_cast<exe_bundle_file*>(stream);
			return f->offset;
		},
		// .zseek64_file =
		[](voidpf, voidpf stream, ZPOS64_T offset, int origin) -> long
		{
			exe_bundle_file * f = reinterpret_cast<exe_bundle_file*>(stream);

			switch (origin)
			{
			case ZLIB_FILEFUNC_SEEK_CUR:
				f->offset += offset;
				break;
			case ZLIB_FILEFUNC_SEEK_END:
				f->offset = f->fsize - offset;
				break;
			case ZLIB_FILEFUNC_SEEK_SET:
				f->offset = offset;
				break;
			default:
				return -1;
			}

			return 0;
		},
		// .zclose_file =
		[](voidpf, voidpf stream) -> int
		{
			exe_bundle_file * f = reinterpret_cast<exe_bundle_file*>(stream);
			boost::checked_delete(f);
			return 0;
		},
		// .zerror_file =
		[](voidpf, voidpf stream) -> int
		{
			exe_bundle_file * f = reinterpret_cast<exe_bundle_file*>(stream);
			return f->error;
		},
		// .opaque
		reinterpret_cast<voidpf>(0)
	};



}
