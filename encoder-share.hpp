#pragma once

#include <obs.h>
#include <cstring>
#include <string>

/* Peer encoder sharing for Aitum Multistream fork.
 *
 * Video/audio encoder fields may use the sentinel "share:<output_name>" to
 * attach to another destination's encoder instance instead of creating a new
 * encode session. Owner destinations keep a normal encoder id (e.g. jim_nvenc).
 *
 * Encoder OBS object names are stable per owner:
 *   aitum_multi_video_encoder_<owner_name>
 *   aitum_multi_audio_encoder_<owner_name>
 * so a second destination can obs_get_encoder_by_name() and reuse.
 */

static constexpr const char *ENCODER_SHARE_PREFIX = "share:";

inline bool encoder_is_share(const char *encoder_id)
{
	return encoder_id && strncmp(encoder_id, ENCODER_SHARE_PREFIX, 6) == 0;
}

inline std::string encoder_share_owner(const char *encoder_id)
{
	if (!encoder_is_share(encoder_id))
		return {};
	return std::string(encoder_id + 6);
}

inline std::string encoder_share_id(const char *owner_name)
{
	if (!owner_name || !owner_name[0])
		return {};
	return std::string(ENCODER_SHARE_PREFIX) + owner_name;
}

obs_data_t *find_output_settings(obs_data_array_t *outputs, const char *name);

/* Resolves video encoder for settings. Returns a strong reference (release
 * after obs_output_set_video_encoder), or nullptr on failure. Sets *error_key
 * to a locale key when returning nullptr (may be null if caller ignores). */
obs_encoder_t *resolve_video_encoder(obs_data_t *settings, obs_data_array_t *outputs, const char **error_key);

obs_encoder_t *resolve_audio_encoder(obs_data_t *settings, obs_data_array_t *outputs, const char **error_key);
