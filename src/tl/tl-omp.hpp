#ifndef TL_OMP_HPP
#define TL_OMP_HPP

#include "tl-compilerphase.hpp"
#include "tl-ast.hpp"
#include "tl-scope.hpp"
#include "tl-scopelink.hpp"
#include "tl-langconstruct.hpp"
#include "tl-handler.hpp"
#include "tl-traverse.hpp"
#include "tl-dto.hpp"

namespace TL
{
	namespace OpenMP
	{
		class Directive;
		class Construct : public LangConstruct
		{
			public:
				Construct(AST_t ref, ScopeLink scope_link)
					: LangConstruct(ref, scope_link)
				{
				}

				Statement body();
				Directive directive();
		};

		class Clause;
		class DefaultClause;
		class ReductionClause;
		class CustomClause;

		class Directive : public LangConstruct
		{
			public:
				Directive(AST_t ref, ScopeLink scope_link)
					: LangConstruct(ref, scope_link)
				{
				}

				Clause nowait_clause();
				Clause num_threads_clause();
				Clause if_clause();

				Clause shared_clause();
				Clause private_clause();
				Clause firstprivate_clause();
				Clause lastprivate_clause();

				Clause copyin_clause();
				Clause copyprivate_clause();

				DefaultClause default_clause();

				ReductionClause reduction_clause();

				CustomClause custom_clause(std::string& src);
		};

		class Clause : public LangConstruct
		{
			private:
				const char* _clause_filter_name;
			public:
				Clause(AST_t ref, 
						ScopeLink scope_link,
						const char* clause_filter_name)
					: LangConstruct(ref, scope_link), _clause_filter_name(clause_filter_name)
				{ 
				}

				ObjectList<IdExpression> id_expressions();
		};

		class DefaultClause : public LangConstruct
		{
			public:
				DefaultClause(AST_t ref, ScopeLink scope_link)
					: LangConstruct(ref, scope_link)
				{
				}

				bool is_none() const;
				bool is_shared() const;
		};

		class ReductionIdExpression
		{
			private:
				IdExpression _symbol;
				AST_t _op;
				AST_t _neuter;
			public:
				ReductionIdExpression(IdExpression s, AST_t op, AST_t neuter)
					: _symbol(s), _op(op), _neuter(neuter)
				{
				}

				AST_t get_id_expression() const
				{
					return _symbol.get_ast();
				}

				Symbol get_symbol() const
				{
					return _symbol.get_symbol();
				}

				AST_t get_neuter() const
				{
					return _neuter;
				}

				AST_t get_operation() const
				{
					return _op;
				}
		};

		class ReductionClause : public LangConstruct
		{
			public:
				ReductionClause(AST_t ref, ScopeLink scope_link)
					: LangConstruct(ref, scope_link)
				{
				}

				ObjectList<ReductionIdExpression> id_expressions();
		};

		class CustomClause : public LangConstruct
		{
			public:
				CustomClause(AST_t ref, ScopeLink scope_link)
					: LangConstruct(ref, scope_link)
				{
				}

				// Pending
		};

		class ParallelConstruct : public Construct
		{
			public:
				ParallelConstruct(AST_t ref, ScopeLink scope_link)
					: Construct(ref, scope_link)
				{
				}
		};

		class ParallelForConstruct : public Construct
		{
			public:
				ParallelForConstruct(AST_t ref, ScopeLink scope_link)
					: Construct(ref, scope_link)
				{
				}
		};

		class OpenMPPhase : public CompilerPhase
		{
			private:
				class ParallelFunctor : public TraverseFunctor
				{
					private:
						OpenMPPhase& _phase;
					public:
						virtual void preorder(Context ctx, AST_t node) 
						{
							ParallelConstruct parallel_construct(node, ctx.scope_link);

							_phase.on_parallel_pre.signal(parallel_construct);
						}

						virtual void postorder(Context ctx, AST_t node) 
						{
							ParallelConstruct parallel_construct(node, ctx.scope_link);
							_phase.on_parallel_post.signal(parallel_construct);
						}

						ParallelFunctor(OpenMPPhase& phase)
							: _phase(phase)
						{
						}
				};

				class ParallelForFunctor : public TraverseFunctor
				{
					private:
						OpenMPPhase& _phase;
					public:
						virtual void preorder(Context ctx, AST_t node) 
						{
							ParallelForConstruct parallel_for_construct(node, ctx.scope_link);

							_phase.on_parallel_for_pre.signal(parallel_for_construct);
						}

						virtual void postorder(Context ctx, AST_t node) 
						{
							ParallelForConstruct parallel_for_construct(node, ctx.scope_link);
							_phase.on_parallel_for_post.signal(parallel_for_construct);
						}

						ParallelForFunctor(OpenMPPhase& phase)
							: _phase(phase)
						{
						}
				};
			protected:
				AST_t translation_unit;
				ScopeLink scope_link;
				Scope global_scope;
			public:
				Signal1<ParallelConstruct> on_parallel_pre;
				Signal1<ParallelConstruct> on_parallel_post;
				
				Signal1<ParallelForConstruct> on_parallel_for_pre;
				Signal1<ParallelForConstruct> on_parallel_for_post;

				virtual void run(DTO& data_flow);
				virtual void init();
		};
	}
}

extern "C"
{
	TL::CompilerPhase* give_compiler_phase_object(void);
}

#endif // TL_OMP_HPP
