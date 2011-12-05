#include <sasl/include/code_generator/llvm/ty_cache.h>

#include <sasl/enums/enums_utility.h>
#include <sasl/enums/default_hasher.h>

using namespace sasl::utility;

BEGIN_NS_SASL_CODE_GENERATOR();

Type* ty_cache_t::type( LLVMContext& ctxt, builtin_types bt, abis abi )
{
	Type*& found_ty = cache[abi][&ctxt][bt];
	if( !found_ty ){ 
		found_ty = create_ty( ctxt, bt, abi );
	}
	return found_ty;
}

std::string const& ty_cache_t::name( builtin_types bt, abis abi )
{
	std::string& ret_name = ty_name[abi][bt];
	if( ret_name.empty() ){
		if( is_scalar(bt) ){
			if( bt == builtin_types::_void ){
				ret_name = "void";
			} else if ( bt == builtin_types::_sint8 ){
				ret_name = "char";
			} else if ( bt == builtin_types::_sint16 ){
				ret_name = "short";
			} else if ( bt == builtin_types::_sint32 ){
				ret_name = "int";
			} else if ( bt == builtin_types::_sint64 ){
				ret_name = "int64";
			} else if ( bt == builtin_types::_uint8 ){
				ret_name = "uchar";
			} else if ( bt == builtin_types::_uint16 ){
				ret_name = "ushort";
			} else if ( bt == builtin_types::_uint32 ){
				ret_name = "uint";
			} else if ( bt == builtin_types::_uint64 ){
				ret_name = "uint64";
			} else if ( bt == builtin_types::_float ){
				ret_name = "float";
			} else if ( bt == builtin_types::_double ){
				ret_name = "double";
			} else if ( bt == builtin_types::_boolean ){
				ret_name = "bool";
			}
		} else if ( is_vector(bt) ){
			ret_name = name( scalar_of(bt), abi );
			ret_name.reserve( ret_name.length() + 5 );
			ret_name += ".v";
			ret_name += lexical_cast<string>( vector_size(bt) );
			ret_name += ( abi == abi_c ? ".c" : ".l" );
		} else if ( is_matrix(bt) ){
			ret_name = name( scalar_of(bt), abi );
			ret_name.reserve( ret_name.length() + 6 );
			ret_name += ".m";
			ret_name += lexical_cast<string>( vector_size(bt) );
			ret_name += lexical_cast<string>( vector_count(bt) );
			ret_name += ( abi == abi_c ? ".c" : ".l" );
		}
	}
	return ret_name;
}

Type* ty_cache_t::create_ty( LLVMContext& ctxt, builtin_types bt, abis abi )
{
	assert( abi == abi_c || abi == abi_llvm );

	if ( is_void( bt ) ){
		return Type::getVoidTy( ctxt );
	}

	if( is_scalar(bt) ){
		if( bt == builtin_types::_boolean ){
			return IntegerType::get( ctxt, 1 );
		}
		if( is_integer(bt) ){
			return IntegerType::get( ctxt, (unsigned int)storage_size( bt ) << 3 );
		}
		if ( bt == builtin_types::_float ){
			return Type::getFloatTy( ctxt );
		}
		if ( bt == builtin_types::_double ){
			return Type::getDoubleTy( ctxt );
		}
	}

	if( is_vector(bt) ){
		Type* elem_ty = type(ctxt, scalar_of(bt), abi );
		size_t vec_size = vector_size(bt);
		if( abi == abi_c ){
			vector<Type*> elem_tys(vec_size, elem_ty);
			return StructType::create( elem_tys, name(bt, abi) );
		} else {
			return VectorType::get( elem_ty, static_cast<unsigned int>(vec_size) );
		}
	}

	if( is_matrix(bt) ){
		Type* vec_ty = type( ctxt, vector_of( scalar_of(bt), vector_size(bt) ), abi );
		vector<Type*> row_tys( vector_count(bt), vec_ty );
		return StructType::create( row_tys, name(bt, abi) );
	}
}

ty_cache_t cache;
Type* get_llvm_type( LLVMContext& ctxt, builtin_types bt, abis abi )
{
	return cache.type( ctxt, bt, abi );
}

END_NS_SASL_CODE_GENERATOR();