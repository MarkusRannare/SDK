#include "c++/schemas/Rating.h"
#include "c/ModioC.h"                      // for ModioRating
#include "dependencies/nlohmann/json.hpp"  // for json

namespace modio
{
void Rating::initialize(ModioRating modio_rating)
{
  game_id = modio_rating.game_id;
  mod_id = modio_rating.mod_id;
  rating = modio_rating.rating;
  date_added = modio_rating.date_added;
}

nlohmann::json toJson(Rating &rating)
{
  nlohmann::json rating_json;

  rating_json["game_id"] = rating.game_id;
  rating_json["mod_id"] = rating.mod_id;
  rating_json["rating"] = rating.rating;
  rating_json["date_added"] = rating.date_added;

  return rating_json;
}
} // namespace modio
