package com.nakamura_labs.ladenzeit.misc;

import java.io.IOException;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.context.annotation.Configuration;

import com.google.api.gax.rpc.FixedHeaderProvider;
import com.google.api.gax.rpc.HeaderProvider;
import com.google.maps.places.v1.PlacesClient;
import com.google.maps.places.v1.PlacesSettings;

@Configuration
public class GoogleMapsConfiguration {
	private static final Logger LOG = LoggerFactory.getLogger(GoogleMapsConfiguration.class);

	@Value("${com.google.maps.api.key}")
	private String apiKey;

	private PlacesClient placesClient(String fields) {
		if (this.apiKey == null || this.apiKey.isBlank()) {
			LOG.error("Google Maps API key not configured.");
			return null;
		}
		
		try {
			HeaderProvider headerProvider = FixedHeaderProvider.create("X-Goog-FieldMask", fields);
			PlacesSettings settings = PlacesSettings.newBuilder().setApiKey(this.apiKey)
					.setHeaderProvider(headerProvider).build();
			return PlacesClient.create(settings);
		} catch (IOException e) {
			LOG.error("Failed to setup places client.", e);
			return null;
		}
	}

	public PlacesClient placesClientCurrentAndRegularOpeningHours() {
		return placesClient("currentOpeningHours,regularOpeningHours,displayName");
	}

	public PlacesClient placesClientSearch() {
		return placesClient("places.id,places.formattedAddress,places.displayName");
	}
}
