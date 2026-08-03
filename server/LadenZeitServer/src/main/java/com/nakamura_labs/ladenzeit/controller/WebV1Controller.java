package com.nakamura_labs.ladenzeit.controller;

import java.util.List;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.http.HttpHeaders;
import org.springframework.http.HttpStatus;
import org.springframework.http.ResponseCookie;
import org.springframework.http.ResponseEntity;
import org.springframework.validation.annotation.Validated;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.ModelAttribute;
import org.springframework.web.bind.annotation.PostMapping;
import org.springframework.web.bind.annotation.PutMapping;
import org.springframework.web.bind.annotation.RequestAttribute;
import org.springframework.web.bind.annotation.RequestBody;
import org.springframework.web.bind.annotation.RequestHeader;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RequestParam;
import org.springframework.web.bind.annotation.RestController;
import org.springframework.web.server.ResponseStatusException;

import com.nakamura_labs.ladenzeit.dto.PlaceSearchEntry;
import com.nakamura_labs.ladenzeit.entity.LZDevice;
import com.nakamura_labs.ladenzeit.entity.LZDevicePlace;
import com.nakamura_labs.ladenzeit.service.LZDeviceService;
import com.nakamura_labs.ladenzeit.service.LZPlaceService;

import jakarta.servlet.http.Cookie;
import jakarta.servlet.http.HttpServletRequest;

@RestController
@RequestMapping("/api/v1/web")
@Validated
public class WebV1Controller {

	private static final Logger LOG = LoggerFactory.getLogger(WebV1Controller.class);

	private static final ResponseStatusException UNAUTHENTICATED = new ResponseStatusException(HttpStatus.UNAUTHORIZED,
			"Invalid Session");
	private static final String COOKIE_SESSION = "SESSION_ID";

	@Value("${com.nakamura_labs.ladenzeit.cookie_expire_hours}")
	private Long cookieExpireHours;

	@Autowired
	private LZDeviceService lzDeviceService;

	@Autowired
	private LZPlaceService lzPlaceService;

	@ModelAttribute
	public void validateSession(HttpServletRequest request) {
		String requestURI = request.getRequestURI();
		LOG.debug("Request: " + requestURI);
		if (requestURI.equals("/api/v1/web/auth/otp")) {
			return;
		}

		Cookie[] cookies = request.getCookies();
		String sessionCookie = null;
		if (cookies != null) {
			for (Cookie cookie : cookies) {
				if (COOKIE_SESSION.equals(cookie.getName())) {
					sessionCookie = cookie.getValue();
				}
			}
		}
		if (sessionCookie == null || sessionCookie.isBlank()) {
			throw UNAUTHENTICATED;
		}

		LZDevice device = lzDeviceService.sessionVerify(sessionCookie);
		if (device == null) {
			throw UNAUTHENTICATED;
		}

		request.setAttribute("device", device);
	}

	@PostMapping("/auth/otp")
	public ResponseEntity<Void> authOtp(@RequestHeader("X-Device-Token") String token) {

		if (token == null || token.isBlank()) {
			throw UNAUTHENTICATED;
		}

		String session = lzDeviceService.sessionCreateByOtp(token);
		if (session == null) {
			throw UNAUTHENTICATED;
		}
		ResponseCookie springCookie = ResponseCookie.from(COOKIE_SESSION, session).httpOnly(true).secure(true)
				.path("/api/v1/web").maxAge(this.cookieExpireHours * 60 * 60).sameSite("Strict").build();

		return ResponseEntity.ok().header(HttpHeaders.SET_COOKIE, springCookie.toString()).build();
	}

	@GetMapping("/auth/status")
	public ResponseEntity<Void> authStatus() {
		return ResponseEntity.ok().build();
	}

	@PostMapping("/auth/invalidate")
	public ResponseEntity<Void> logout(@RequestAttribute LZDevice device) {
		lzDeviceService.sessionInvalidate(device);
		return ResponseEntity.ok().build();
	}

	@GetMapping("/places")
	public ResponseEntity<List<LZDevicePlace>> placesGet(@RequestAttribute LZDevice device) {
		List<LZDevicePlace> places = device.getDevicePlaces();
		return ResponseEntity.ok(places);
	}

	@PutMapping("/places")
	public ResponseEntity<List<LZDevicePlace>> placesPut(@RequestAttribute LZDevice device,
			@RequestBody List<LZDevicePlace> newPlaces) {
		lzDeviceService.replacePlaces(device, newPlaces);
		return placesGet(device);
	}

	@GetMapping("/search-google")
	public ResponseEntity<List<PlaceSearchEntry>> searchGoogle(@RequestAttribute LZDevice device,
			@RequestParam String query) {
		if (query == null || query.trim().length() < 3) {
			return ResponseEntity.ok().build();
		}
		List<PlaceSearchEntry> places = lzPlaceService.searchPlacesFromGoogle(query);
		return ResponseEntity.ok(places);
	}

}