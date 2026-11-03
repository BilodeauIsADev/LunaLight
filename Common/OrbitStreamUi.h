#pragma once

// Pure helpers for the stream page's failure and end-of-session screens.
// Header-only so tests can compile them without the UWP project.

#include <climits>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace moonlight_xbox_dx {
namespace OrbitStreamUi {

	// Bitrate slider stops: fine where most streams sit, coarse near the maximum.
	const int MaxBitrateStep = 47;

	inline int BitrateForStep(int step) {
		int kbps = 5000;
		for (int i = 0; i < step && i < MaxBitrateStep; ++i) {
			kbps += kbps < 100000 ? 5000 : kbps < 200000 ? 10000 : kbps < 500000 ? 25000 : 50000;
		}
		return kbps;
	}

	// The stop to show for a bitrate, which may lie between stops: codec defaults do.
	inline int NearestBitrateStep(int kbps) {
		int best = 0;
		int bestDistance = INT_MAX;
		for (int step = 0; step <= MaxBitrateStep; ++step) {
			const int distance = std::abs(BitrateForStep(step) - kbps);
			if (distance < bestDistance) {
				best = step;
				bestDistance = distance;
			}
		}
		return best;
	}

	struct StreamPreset {
		int maxHeight;
		int fps;
		bool immediatePacing;
		const wchar_t* audioBuffer;
	};

	// In list order: 0 = Best quality, 1 = Balanced, 2 = Low latency.
	inline StreamPreset GetStreamPreset(int preset, int displayHeight, bool supports120Hz, bool xboxOne) {
		const bool lowLatency = preset == 2;
		StreamPreset result;
		result.maxHeight = preset == 0 || displayHeight < 1080 ? displayHeight : 1080;
		result.fps = lowLatency && supports120Hz ? 120 : 60;
		// Matches the frame pacing advice in the settings: Display-locked suits Xbox One
		result.immediatePacing = lowLatency || !xboxOne;
		result.audioBuffer = lowLatency ? L"20 ms" : L"30 ms";
		return result;
	}

	// Error codes passed to MoonlightClient::OnFailed, mirrored from
	// moonlight-common-c (Limelight.h), libgamestream (errors.h) and the
	// Windows Sockets codes surfaced by failed connection stages. The mirror
	// keeps this header free of those C headers.
	enum StartError {
		StartErrorGracefulTermination = 0,
		// libgamestream
		StartErrorGsFailed = -1,
		StartErrorGsOutOfMemory = -2,
		StartErrorGsInvalid = -3,
		StartErrorGsWrongState = -4,
		StartErrorGsIoError = -5,
		StartErrorGsNotSupported4K = -6,
		StartErrorGsUnsupportedVersion = -7,
		StartErrorGsNotSupportedMode = -8,
		StartErrorGsError = -9,
		StartErrorGsNotSupportedSopsResolution = -10,
		// moonlight-common-c
		StartErrorNoVideoTraffic = -100,
		StartErrorNoVideoFrame = -101,
		StartErrorUnexpectedEarlyTermination = -102,
		StartErrorProtectedContent = -103,
		StartErrorFrameConversion = -104,
		// Windows Sockets
		StartErrorNetworkDown = 10051,
		StartErrorSocketTimeout = 10060,
		StartErrorConnectionRefused = 10061,
		StartErrorHostUnreachable = 10065,
		StartErrorHostNotFound = 11001,
	};

	// Trims whitespace (including the trailing newlines the stage-failure
	// message carries) from both ends of a log line.
	inline std::string Trimmed(const std::string& text) {
		const char* whitespace = " \t\r\n";
		size_t begin = text.find_first_not_of(whitespace);
		if (begin == std::string::npos) return std::string();
		size_t end = text.find_last_not_of(whitespace);
		return text.substr(begin, end - begin + 1);
	}

	// Plain-words reason for a failed stream start. Known codes map to readable
	// text; anything else falls back to the raw message, then to a generic line.
	inline std::string DescribeStartFailure(int error, const std::string& rawMessage) {
		const char* known = nullptr;
		switch (error) {
		case StartErrorGsNotSupported4K:
			known = "The host can't stream at 4K. Lower the resolution in the stream settings.";
			break;
		case StartErrorGsUnsupportedVersion:
			known = "The host's streaming software is too old. Update GeForce Experience or Sunshine on it.";
			break;
		case StartErrorGsNotSupportedMode:
			known = "The host doesn't support the selected combination of codec, resolution or frame rate.";
			break;
		case StartErrorGsNotSupportedSopsResolution:
			known = "The host can't stream this app at the selected resolution. Try a lower one.";
			break;
		case StartErrorNoVideoTraffic:
			known = "The host was reachable but never sent video. Check its firewall and port forwarding rules.";
			break;
		case StartErrorNoVideoFrame:
			known = "The connection was too unstable to receive a complete video frame. Try a lower bitrate.";
			break;
		case StartErrorUnexpectedEarlyTermination:
			known = "The host ended the stream right after it started. DRM-protected content on the host can cause this.";
			break;
		case StartErrorProtectedContent:
			known = "The host can't capture protected content. Close any DRM-protected video playing on it.";
			break;
		case StartErrorFrameConversion:
			known = "The host couldn't convert its video for this stream. Try a different resolution or turn HDR off.";
			break;
		case StartErrorNetworkDown:
			known = "The console's network connection is down.";
			break;
		case StartErrorSocketTimeout:
			known = "The connection to the host timed out. Make sure the host is awake and the address is right.";
			break;
		case StartErrorConnectionRefused:
			known = "The host refused the connection. Make sure its streaming software is running.";
			break;
		case StartErrorHostUnreachable:
			known = "The host couldn't be reached over the network.";
			break;
		case StartErrorHostNotFound:
			known = "The host's address couldn't be resolved. Check it in the host settings.";
			break;
		default:
			break;
		}
		if (known != nullptr) return known;

		std::string trimmed = Trimmed(rawMessage);
		if (!trimmed.empty()) return trimmed;
		return "The stream couldn't be started. Try again.";
	}

	// Wall-clock length of the session: "1h 42m", "5m 07s", "42s".
	inline std::string FormatPlayedDuration(long long seconds) {
		if (seconds < 0) seconds = 0;
		const long long hours = seconds / 3600;
		const long long minutes = (seconds % 3600) / 60;
		const long long secs = seconds % 60;
		char text[32];
		if (hours > 0) {
			if (minutes > 0) {
				snprintf(text, sizeof(text), "%lldh %lldm", hours, minutes);
			} else {
				snprintf(text, sizeof(text), "%lldh", hours);
			}
		} else if (minutes > 0) {
			snprintf(text, sizeof(text), "%lldm %02llds", minutes, secs);
		} else {
			snprintf(text, sizeof(text), "%llds", secs);
		}
		return std::string(text);
	}

	// The end-of-session card shows only for clean endings that lasted a
	// moment; errors and blips keep the plain navigate-back.
	inline bool ShouldShowSessionSummary(long long durationSeconds, bool endedNormally) {
		return endedNormally && durationSeconds >= 10;
	}

} // namespace OrbitStreamUi
} // namespace moonlight_xbox_dx
