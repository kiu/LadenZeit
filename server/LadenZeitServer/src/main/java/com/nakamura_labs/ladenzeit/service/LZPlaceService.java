package com.nakamura_labs.ladenzeit.service;

import java.time.Instant;
import java.time.temporal.ChronoUnit;
import java.util.List;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.boot.context.event.ApplicationReadyEvent;
import org.springframework.context.event.EventListener;
import org.springframework.scheduling.annotation.Scheduled;
import org.springframework.stereotype.Service;

import com.google.api.gax.rpc.InvalidArgumentException;
import com.google.maps.places.v1.GetPlaceRequest;
import com.google.maps.places.v1.Place;
import com.google.maps.places.v1.Place.OpeningHours;
import com.google.maps.places.v1.Place.OpeningHours.Period;
import com.google.maps.places.v1.Place.OpeningHours.Period.Point;
import com.google.maps.places.v1.PlacesClient;
import com.google.maps.places.v1.SearchTextRequest;
import com.google.maps.places.v1.SearchTextResponse;
import com.nakamura_labs.ladenzeit.dto.PlaceSearchEntry;
import com.nakamura_labs.ladenzeit.entity.LZPlace;
import com.nakamura_labs.ladenzeit.entity.LZPlaceHour;
import com.nakamura_labs.ladenzeit.misc.GoogleMapsConfiguration;
import com.nakamura_labs.ladenzeit.repository.LZPlaceRepository;

@Service
public class LZPlaceService {
	private static final Logger LOG = LoggerFactory.getLogger(LZPlaceService.class);

	@Autowired
	private GoogleMapsConfiguration maps;

	@Autowired
	private LZPlaceRepository lzPlaceRepository;

	@Value("${com.nakamura_labs.ladenzeit.place_cache_hours}")
	private Integer placeCacheHours;

	@Scheduled(fixedDelay = 1000 * 60 * 60)
	@EventListener(ApplicationReadyEvent.class)
	protected void cleanCache() {
		LOG.info("Cache clearing entries older " + this.placeCacheHours + " hours.");
		Instant threshold = Instant.now().minus(this.placeCacheHours, ChronoUnit.HOURS);
		lzPlaceRepository.deleteByCreatedAtBefore(threshold);
		LOG.info("Cache clearing completed.");
	}

	public LZPlace getPlace(String placeId) {
		LZPlace lzp = lzPlaceRepository.findById(placeId).orElse(null);

		if (lzp == null) {
			LOG.debug("Cache miss for place: " + placeId);
			lzp = updatePlaceFromGoogle(placeId);
		} else {
			LOG.debug("Cache hit for place: " + placeId);
		}

		return lzp;
	}

	private String pointToString(Point p) {
		return String.format("%02d:%02d", p.getHour(), p.getMinute());
	}

	private int toMinutes(Point p) {
		int min = 0;
		min += p.getDay() * 1440;
		min += p.getHour() * 60;
		min += p.getMinute();
		return min;
	}

	private LZPlace updatePlaceFromGoogle(String placeId) {
		LZPlace lzp = new LZPlace();
		lzp.setId(placeId);

		PlacesClient placesClient = maps.placesClientCurrentAndRegularOpeningHours();
		if (placesClient == null) {
			LOG.warn("Google place lookup failed due to failure in placesClient: " + placeId);
			return null;
		}

		GetPlaceRequest request = GetPlaceRequest.newBuilder().setName("places/" + placeId).build();
		try {
			Place place = placesClient.getPlace(request);

			lzp.setName(place.getDisplayName().getText());

			OpeningHours oh = null;
			if (place.hasRegularOpeningHours()) {
				oh = place.getRegularOpeningHours();
			}
			if (place.hasCurrentOpeningHours()) {
				oh = place.getCurrentOpeningHours();
			}

			if (oh == null) {
				LOG.warn("Place has no opening hours: " + placeId);
			} else {
				for (Period period : oh.getPeriodsList()) {
					if (!period.hasOpen() || !period.hasClose()) {
						LOG.warn("Place has missing open/close:\n" + period);
						continue;
					}

					LZPlaceHour lzph = new LZPlaceHour();
					lzph.setFromString(pointToString(period.getOpen()));
					lzph.setFromWeekday(period.getOpen().getDay());
					lzph.setFromInteger(toMinutes(period.getOpen()));

					lzph.setToString(pointToString(period.getClose()));
					lzph.setToWeekday(period.getClose().getDay());
					lzph.setToInteger(toMinutes(period.getClose()));

					lzp.addPlaceHour(lzph);
				}
			}
		} catch (InvalidArgumentException e) {
			LOG.error("Error parsing Google Maps response.", e);
			return null;
		}

		lzPlaceRepository.save(lzp);
		return lzp;
	}

	public List<PlaceSearchEntry> searchPlacesFromGoogle(String query) {
		PlacesClient placesClient = maps.placesClientSearch();
		if (placesClient == null) {
			LOG.warn("Google search failed due to failure in placesClient.");
			return null;
		}
		SearchTextRequest request = SearchTextRequest.newBuilder().setTextQuery(query).setMaxResultCount(10).build();
		SearchTextResponse response = placesClient.searchText(request);

		return response.getPlacesList().stream()
				.map(place -> new PlaceSearchEntry(place.getId(),
						place.hasDisplayName() ? place.getDisplayName().getText() : "Unknown",
						place.getFormattedAddress()))
				.toList();
	}

}
