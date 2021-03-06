#include "c++/methods/callbacks/ModInstanceCallbacks.h"
#include "c++/schemas/Response.h"

namespace modio
{
std::map<u32, GetModCall *> get_mod_calls;
std::map<u32, GetAllModsCall *> get_all_mods_calls;
std::map<u32, AddModCall *> add_mod_calls;
std::map<u32, EditModCall *> edit_mod_calls;
std::map<u32, GenericCall *> delete_mod_calls;

void onGetMod(void *object, ModioResponse modio_response, ModioMod mod)
{
  u32 call_id = (u32)((uintptr_t)object);

  modio::Response response;
  response.initialize(modio_response);

  modio::Mod modio_mod;

  if (modio_response.code == 200)
  {
    modio_mod.initialize(mod);
  }

  get_mod_calls[call_id]->callback(response, modio_mod);

  delete get_mod_calls[call_id];
  get_mod_calls.erase(call_id);
}

void onGetAllMods(void *object, ModioResponse modio_response, ModioMod mods[], u32 mods_size)
{
  u32 call_id = (u32)((uintptr_t)object);

  modio::Response response;
  response.initialize(modio_response);

  std::vector<modio::Mod> mods_vector;
  mods_vector.resize(mods_size);
  for (u32 i = 0; i < mods_size; i++)
  {
    mods_vector[i].initialize(mods[i]);
  }

  get_all_mods_calls[call_id]->callback(response, mods_vector);

  delete get_all_mods_calls[call_id];
  get_all_mods_calls.erase(call_id);
}

void onAddMod(void *object, ModioResponse modio_response, ModioMod mod)
{
  u32 call_id = (u32)((uintptr_t)object);

  modio::Response response;
  response.initialize(modio_response);

  modio::Mod modio_mod;

  if (modio_response.code == 201)
  {
    modio_mod.initialize(mod);
  }

  add_mod_calls[call_id]->callback(response, modio_mod);

  delete add_mod_calls[call_id];
  add_mod_calls.erase(call_id);
}

void onEditMod(void *object, ModioResponse modio_response, ModioMod mod)
{
  u32 call_id = (u32)((uintptr_t)object);

  modio::Response response;
  response.initialize(modio_response);

  modio::Mod modio_mod;

  if (modio_response.code == 200)
  {
    modio_mod.initialize(mod);
  }

  edit_mod_calls[call_id]->callback(response, modio_mod);

  delete edit_mod_calls[call_id];
  edit_mod_calls.erase(call_id);
}

void onDeleteMod(void *object, ModioResponse modio_response)
{
  u32 call_id = (u32)((uintptr_t)object);

  modio::Response response;
  response.initialize(modio_response);

  delete_mod_calls[call_id]->callback(response);

  delete delete_mod_calls[call_id];
  delete_mod_calls.erase(call_id);
}

void clearModRequestCalls()
{
  for (auto get_mod_call : get_mod_calls)
    delete get_mod_call.second;
  get_mod_calls.clear();

  for (auto get_all_mods_call : get_all_mods_calls)
    delete get_all_mods_call.second;
  get_all_mods_calls.clear();

  for (auto add_mod_call : add_mod_calls)
    delete add_mod_call.second;
  add_mod_calls.clear();

  for (auto edit_mod_call : edit_mod_calls)
    delete edit_mod_call.second;
  edit_mod_calls.clear();

  for (auto delete_mod_call : delete_mod_calls)
    delete delete_mod_call.second;
  delete_mod_calls.clear();
}
} // namespace modio
