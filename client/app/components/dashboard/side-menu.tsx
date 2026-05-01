'use client'
import * as React from 'react';
import Typography from '@mui/material/Typography';
import Grid from '@mui/material/Grid';
import List from '@mui/material/List';
import ListItem from '@mui/material/ListItem';
import ListItemButton from '@mui/material/ListItemButton';
import ListItemText from '@mui/material/ListItemText';
import VideoIcon from '@mui/icons-material/Movie';
import MusicIcon from '@mui/icons-material/MusicNote';
import ImageIcon from '@mui/icons-material/Image';
import ListItemAvatar from '@mui/material/ListItemAvatar';
import Avatar from '@mui/material/Avatar';
import Divider from '@mui/material/Divider';
import Button from '@mui/material/Button';
import AddIcon from '@mui/icons-material/Add';
import AddMediaDialog from '@/app/components/dashboard/add-media-dialog';
import { MediaTypeFilter } from '@/app/page';

type Props = {
  selectedType: MediaTypeFilter;
  onSelectType: (t: MediaTypeFilter) => void;
};

const entries: {
  type: MediaTypeFilter;
  label: string;
  Icon: React.ElementType;
  paletteKey: 'video' | 'music' | 'image';
}[] = [
  { type: 'video', label: 'Videos', Icon: VideoIcon, paletteKey: 'video' },
  { type: 'audio', label: 'Music',  Icon: MusicIcon, paletteKey: 'music' },
  { type: 'image', label: 'Images', Icon: ImageIcon, paletteKey: 'image' },
];

export default function SideMenu({ selectedType, onSelectType }: Props) {
  const [addDialogOpen, setAddDialogOpen] = React.useState(false);

  return (
    <Grid sx={{ maxWidth: 350, bgcolor: 'background.paper', p: 1, borderRadius: 3 }}>
      <Grid container sx={{ justifyContent: 'space-between', ml: .5, mr: .5 }}>
        <Typography component="h5" variant="h5" gutterBottom sx={{ pl: 2, pt: 1, pb: .5 }}>
          Library
        </Typography>
        <Button variant="contained" size="small" startIcon={<AddIcon />} onClick={() => setAddDialogOpen(true)} sx={{ bgcolor: "secondary.main", color: "primary.contrastText", ml: 2, mr: 2, mt: 1, mb: 1, pl: 7, pr: 7 }}>
          Add
        </Button>
      </Grid>
      <Grid sx={{ maxWidth: 300, bgcolor: 'primary.main', margin: 2, borderRadius: 2 }}>
        <nav aria-label="media types">
          <List>
            {entries.map(({ type, label, Icon, paletteKey }, idx) => (
              <React.Fragment key={type}>
                <ListItem alignItems="flex-start">
                  <ListItemButton selected={selectedType === type} onClick={() => onSelectType(type)} sx={{ width: '100%', maxWidth: 300, bgcolor: 'primary.main', '&:hover': { bgcolor: `${paletteKey}.light` }, color: `${paletteKey}.contrastText`, borderRadius: 2,}}>
                    <ListItemAvatar>
                      <Avatar sx={{ bgcolor: `${paletteKey}.main` }}>
                        <Icon sx={{ color: `${paletteKey}.contrastText` }} />
                      </Avatar>
                    </ListItemAvatar>
                    <ListItemText primary={label} />
                  </ListItemButton>
                </ListItem>
                {idx < entries.length - 1 && (
                  <Divider aria-hidden="true" variant="middle" component="li" sx={{ color: 'divider' }} />
                )}
              </React.Fragment>
            ))}
          </List>
        </nav>
      </Grid>
      <AddMediaDialog
        open={addDialogOpen}
        onClose={() => setAddDialogOpen(false)}
      />
    </Grid>
  );
}